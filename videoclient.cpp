#include "videoclient.h"

VideoClient::VideoClient()
{
}

VideoClient::~VideoClient()
{
    stopSocketConnection();
}

void VideoClient::startSocketConnection(const NetConnectInfo &netConnectInfo)
{
#ifdef PLATFORM_WINDOWS
    WORD versionRequested;
    WSADATA wsaData;
    int err;
    versionRequested = MAKEWORD(2, 2);
    err = WSAStartup(versionRequested, &wsaData);
    if (err != 0) {
        std::cerr << "Load WinSock Failed" << std::endl;
        return;
    }
#endif
    // 创建套接字
    m_socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socketFD < 0)
    {
        std::cerr << "client socket create failed" << std::endl;
        return;
    }

    struct sockaddr_in sockAddrIn;
    memset(&sockAddrIn, 0, sizeof(struct sockaddr_in));

    sockAddrIn.sin_family = AF_INET;
    sockAddrIn.sin_port = htons(netConnectInfo.m_port);
    sockAddrIn.sin_addr.s_addr = inet_addr(netConnectInfo.m_serverIP.c_str());

    // // 将IPv4地址字符串转换为二进制地址缓冲区指针,存储在前面定义的协议的地址中
    // if (inet_pton(AF_INET, netConnectInfo.m_serverIP.c_str(), &sockAddrIn.sin_addr.s_addr) < 0)
    // {
    //     std::cerr << "convert IPv4 address string to binary buffer pointer failed";
    //     return;
    // }

#ifdef PLATFORM_LINUX
    // 获取文件描述符当前存在的标志
    int flags = fcntl(m_socketFD, F_GETFL, 0);
    // 将非阻塞添加进去,这样后面的连接操作不会等待连接完成,而是直接返回,避免阻塞其他操作
    fcntl(m_socketFD, F_SETFL, flags | O_NONBLOCK);
#elif PLATFORM_WINDOWS
    unsigned long ul = 1;
    ioctlsocket(m_socketFD, FIONBIO, &ul);
#endif

    connect(m_socketFD, reinterpret_cast<struct sockaddr *>(&sockAddrIn), sizeof(struct sockaddr));

    // 单独用一个线程来监听连接有没有完成并判断状态
    std::thread runWaitConnectionThread([this]()
                                        { this->doRunWaitConnection(); });
    runWaitConnectionThread.detach();

    // 再用一个线程接收服务端发来的实际的数据包
    std::thread receiveDataThread([this]()
                                  { this->doReceiveData(); });
    receiveDataThread.detach();

    // 发送心跳包，提醒服务端，此客户端的存活状态
    // 这是另一种更简单的写法，传入成员函数和对象指针
    std::thread sendAlivePacketThread(&VideoClient::sendKeepAlivePacket, this);
    sendAlivePacketThread.detach();
}

void VideoClient::stopSocketConnection()
{
    m_isThreadRunning = false;
    if (m_socketFD >= 0)
    {
        close(m_socketFD);
        m_socketFD = -1;
    }

    // 析构时可阻塞等待线程执行完再退出
    while (m_isKeepAliveThreadRunning)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    while (m_isReceiveThreadRunning)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void VideoClient::setupUpdateVideoCallback(updateVideoCallback &&callback)
{
    m_updateVideoCallback = callback;
}

void VideoClient::doRunWaitConnection()
{
    // 文件描述符集合类型
    // 分别创建读集合和写集合监听套接字的可读和可写状态
    fd_set rSet, wSet;

    // 清空集合
    FD_ZERO(&rSet);
    FD_ZERO(&wSet);

    // 设置超时时间为10秒+0微秒
    struct timeval timeout = {1000, 0};
#ifdef _WIN32
    SOCKET winSocket = static_cast<SOCKET>(m_socketFD);
    FD_SET(winSocket, &rSet);
    FD_SET(winSocket, &wSet);
    int retValue = select(0, &rSet, &wSet, nullptr, &timeout);
#else
    // 将描述符放到集合中
    FD_SET(m_socketFD, &rSet);
    FD_SET(m_socketFD, &wSet);

    // 调用select函数等待套接字变成可读或可写状态
    // 第一个参数传入集合中的最大描述符+1,表示select需要检查范围的上限
    // 后面分别传入读集合和写集合来监听
    // 第四个参数传入nullptr表示不监视异常情况
    // 最后一个参数表示超时时间
    int retValue = select(m_socketFD + 1, &rSet, &wSet, nullptr, &timeout);
#endif

#ifdef _WIN32
    if (retValue == SOCKET_ERROR)
    {
        std::cerr << "select failed, error: " << WSAGetLastError() << std::endl;
#else
    if (retValue == -1)
    {
        // 错误
        std::cerr << "select called failed" << std::endl;
#endif
        return;
    }
    else if (retValue == 0)
    {
        // 超时
        std::cerr << "select is time out" << std::endl;
        return;
    }

#ifdef _WIN32
    if (!FD_ISSET(winSocket, &wSet)) {
#else
    // 检查套接字是否在写集合中(连接完成的关键标志,不在直接return)
    if (!FD_ISSET(m_socketFD, &wSet)) {
#endif
        std::cerr << "no write set" << std::endl;
        return;
    }

    // 获取套接字错误状态去进一步判断
    int error = 0;
    socklen_t len = sizeof(error);
#ifdef _WIN32
    if (getsockopt(winSocket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len) == SOCKET_ERROR) {
        std::cerr << "getsockopt failed, error: " << WSAGetLastError() << std::endl;
#else
    if (getsockopt(m_socketFD, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        std::cerr << "getsockopt failed" << std::endl;
#endif
        return;
    }
    if (error != 0)
    {
        std::cerr << "connect failed: " << strerror(error) << std::endl;
        return;
    }

    m_isConnected = true;
    std::cout << "connect success" << std::endl;
}

void VideoClient::doReceiveData()
{
    H264Decoder decoder;
    while (m_isThreadRunning)
    {
        m_isReceiveThreadRunning = true;
        // 10ms收取1次，这里不能接收太慢，如果频率太慢会导致大量socket数据丢弃，接收到的数据就是不连续的
        // 会导致解码不出来
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!m_isConnected)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        std::vector<uint8_t> buffer(sizeof(NetMessageHeader));
        if (!receiveSocketData(buffer, sizeof(NetMessageHeader)))
        {
            std::cerr << "failed to receive message header" << std::endl;
            continue;
        }

        // 将接收到的数据转为需要的信息头结构体
        NetMessageHeader msgHeader;
        memcpy(&msgHeader, buffer.data(), sizeof(NetMessageHeader));

        // 匹配消息头
        if (strncmp(msgHeader.m_headerID, "ALIVE", 5) != 0 || msgHeader.m_msgType != MSGHEADER_TYPE_STREAM || msgHeader.m_subType != MSGHEADER_STREAM_VIDEO)
        {
            continue;
        }
        // 消息头匹配成功再处理流媒体包

        // 根据传过来的消息头中记录的数据的大小设置空间
        std::vector<uint8_t> streamBuffer(msgHeader.m_length);
        receiveSocketData(streamBuffer, msgHeader.m_length);

        YUVFrameData yuvFrameData;
        int ret = decoder.decodeH264Packet(std::move(streamBuffer), msgHeader.m_length, &yuvFrameData);
        if (ret != 0)
        {
            continue;
        }
        m_updateVideoCallback(&yuvFrameData);
    }
    m_isReceiveThreadRunning = false;
    std::cout << "stop receive packet from server" << std::endl;
}

// 发送心跳包，告诉服务端，此客户端还活着
// 避免客户端非正常结束，服务端接收不到close信号
// 检测不到心跳包就直接关闭和此客户端的连接
void VideoClient::sendKeepAlivePacket()
{
    while (m_isThreadRunning)
    {
        m_isKeepAliveThreadRunning = true;
        std::cout << "send alive packet..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // 没连接上的时候先空转
        if (!m_isConnected)
        {
            continue;
        }

        // 这里心跳包后两个参数都是不用的
        NetMessageHeader msgHeader("ALIVE", MSGHEADER_TYPE_KEEPALIVE, 0, 0);

        // 将要传入的对象转换为字节容器的形式
        std::vector<uint8_t> buffer;
        buffer.resize(sizeof(NetMessageHeader));
        memcpy(buffer.data(), &msgHeader, sizeof(NetMessageHeader));

        // 发送数据
        if (!sendSocketData(buffer, sizeof(NetMessageHeader)))
        {
            std::cerr << "failed to send message header" << std::endl;
        }
    }
    m_isKeepAliveThreadRunning = false;

    std::cout << "stop send alive packet" << std::endl;
}
#ifdef PLATFORM_LINUX
bool VideoClient::receiveSocketData(std::vector<uint8_t> &buffer, size_t length)
{

    // 第一个信号表示向已关闭的socket写入数据，一般会异常终止进程
    // 第二个信号表示忽略第一个信号，避免终止
    std::signal(SIGPIPE, SIG_IGN);


    std::lock_guard<std::mutex> lock(m_receiveMutex);

    // 调整buffer大小以容纳指定长度的数据
    buffer.resize(length);

    // 已成功接收到的数据
    size_t receiveLength = 0;
    // 循环接收数据直到达到指定长度
    while (receiveLength < length)
    {
        // 从socket接收数据，buffer.data() + receiveLength代表从已经接收到的数据的后面开始接收
        // length - receiveLength表示总共需要的-已经接收的=剩下还需要多少=这次最大接收多少
        // 最后一个参数代表，模式，0为默认值，代表阻塞模式等待数据到达，但因为之前设置了非阻塞模式，所以这里还是非阻塞
        // 返回接收数据大小
        int nRet = recv(m_socketFD, buffer.data() + receiveLength, length - receiveLength, 0);

        // 异常处理
        if (nRet < 0)
        {
            // 第一个和第三个表示socket无数据可读，第二个表示系统调用被中断
            // 这三种情况都先短暂休眠
            if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // 其他错误直接返回false
            std::cerr << "socket receive error" << std::endl;
            return false;
        }

        // 对方连接关闭则直接返回false
        if (nRet == 0)
        {
            std::cerr << "connection close, socket receive error" << std::endl;
            return false;
        }

        receiveLength += nRet;
    }

    return true;
}

bool VideoClient::sendSocketData(const std::vector<uint8_t> &buffer, size_t length)
{
    // 忽略SIGPIPE信号，防止向已关闭的socket写入数据时程序异常终止
    std::signal(SIGPIPE, SIG_IGN);

    std::lock_guard<std::mutex> lock(m_sendMutex);

    // 不需要调整buffer大小，因为是发送数据，buffer应该是已经准备好的数据
    // buffer应该至少包含length字节的数据

    // 已成功发送的数据
    size_t sendLength = 0;
    // 循环发送数据直到达到指定长度
    while (sendLength < length)
    {
        // 从buffer中发送数据，buffer.data() + sendLength代表从未发送的数据开始发送
        // length - sendLength表示总共需要发送的-已经发送的=剩下还需要发送多少
        // 最后一个参数0为默认值，表示使用默认行为
        // send函数返回实际发送的字节数
        int nRet = send(m_socketFD, buffer.data() + sendLength, length - sendLength, 0);

        // 异常处理
        if (nRet < 0)
        {
            // 第一个和第三个表示socket无数据可读，第二个表示系统调用被中断
            // 这三种情况都先短暂休眠
            if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // 其他错误则直接返回false
            std::cerr << "socket send error" << std::endl;
            return false;
        }

        // 对方连接关闭则直接返回false
        if (nRet == 0)
        {
            std::cerr << "connection close, socket send error" << std::endl;
            return false;
        }

        // 累加已发送的数据长度
        sendLength += nRet;
    }

    return true;
}

#elif PLATFORM_WINDOWS
bool VideoClient::receiveSocketData(std::vector<uint8_t> &buffer, size_t length)
{
    std::lock_guard<std::mutex> lock(m_receiveMutex);

    // 调整buffer大小以容纳指定长度的数据
    buffer.resize(length);

    // 已成功接收到的数据
    size_t receiveLength = 0;
    // 循环接收数据直到达到指定长度
    while (receiveLength < length)
    {
        // 从socket接收数据，buffer.data() + receiveLength代表从已经接收到的数据的后面开始接收
        // length - receiveLength表示总共需要的-已经接收的=剩下还需要多少=这次最大接收多少
        // 最后一个参数代表，模式，0为默认值，代表阻塞模式等待数据到达，但因为之前设置了非阻塞模式，所以这里还是非阻塞
        // 返回接收数据大小
        int nRet = recv(m_socketFD, reinterpret_cast<char*>(buffer.data() + receiveLength), static_cast<int>(length - receiveLength), 0);

        // 异常处理 - Windows使用不同的错误检查方式
        if (nRet == SOCKET_ERROR)
        {
            int errorCode = WSAGetLastError();

            // Windows下的非阻塞错误码
            if (errorCode == WSAEWOULDBLOCK || errorCode == WSAEINTR)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // 其他错误
            std::cerr << "socket receive error, code: " << errorCode << std::endl;
            return false;
        }

        // 对方连接关闭
        if (nRet == 0)
        {
            std::cerr << "connection close, socket receive error" << std::endl;
            return false;
        }

        receiveLength += nRet;
    }

    return true;
}

bool VideoClient::sendSocketData(const std::vector<uint8_t> &buffer, size_t length)
{
    std::lock_guard<std::mutex> lock(m_sendMutex);

    // 不需要调整buffer大小，因为是发送数据，buffer应该是已经准备好的数据
    // buffer应该至少包含length字节的数据

    // 已成功发送的数据
    size_t sendLength = 0;
    // 循环发送数据直到达到指定长度
    while (sendLength < length)
    {
        // Windows版本：需要类型转换和长度转换
        int nRet = send(m_socketFD,
                        reinterpret_cast<const char*>(buffer.data() + sendLength),
                        static_cast<int>(length - sendLength),
                        0);

        // 异常处理 - Windows使用不同的错误检查方式
        if (nRet == SOCKET_ERROR)
        {
            int errorCode = WSAGetLastError();

            // Windows下的非阻塞错误码
            if (errorCode == WSAEWOULDBLOCK || errorCode == WSAEINTR)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // 其他错误
            std::cerr << "Windows socket send error, code: " << errorCode << std::endl;
            return false;
        }

        // 对方连接关闭则直接返回false
        if (nRet == 0)
        {
            std::cerr << "connection close, socket send error" << std::endl;
            return false;
        }

        // 累加已发送的数据长度
        sendLength += nRet;
    }

    return true;
}

#endif


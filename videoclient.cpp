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
#ifdef _WIN32
    // 初始化WinSock库
    WORD versionRequested; // 版本号
    WSADATA wsaData;       // 用于存储WinSock初始化信息的结构体

    int err;
    versionRequested = MAKEWORD(2, 2);            // 请求使用WinSock 2.2版本
    err = WSAStartup(versionRequested, &wsaData); // 初始化WinSock库
    if (err != 0)
    {
        std::cerr << "Load WinSock Failed" << std::endl;
        return;
    }
#endif
    /*
    创建套接字
    AF_INET: 使用IPv4地址族
    AF_INET+*SOCK_STREAM: 使用面向连接的TCP协议
    0: 默认协议，对于AF_INET和SOCK_STREAM组合，默认是TCP协议
    */
    m_socketFD = socket(AF_INET, SOCK_STREAM, 0);

#ifdef _WIN32
    if (m_socketFD == INVALID_SOCKET)
    {
        std::cerr << "client socket create failed, error: " << WSAGetLastError() << std::endl;
        return;
    }
#elif defined(__linux__)
    if (m_socketFD < 0)
    {
        std::cerr << "client socket create failed" << std::endl;
        return;
    }
#endif

    // 设置套接字地址结构体
    struct sockaddr_in sockAddrIn;
    memset(&sockAddrIn, 0, sizeof(struct sockaddr_in));

    sockAddrIn.sin_family = AF_INET;                    // IPv4地址族
    sockAddrIn.sin_port = htons(netConnectInfo.m_port); // 设置端口号，使用网络字节序

    // inet_addr的作用是将点分十进制的IPv4地址转换为一个无符号长整型数值，此方式已过时
    // sockAddrIn.sin_addr.s_addr = inet_addr(netConnectInfo.m_serverIP.c_str());

    // 将IPv4地址字符串转换为二进制地址缓冲区指针,存储在前面定义的协议的地址中
    if (inet_pton(AF_INET, netConnectInfo.m_serverIP.c_str(), &sockAddrIn.sin_addr.s_addr) < 0)
    {
        std::cerr << "convert IPv4 address string to binary buffer pointer failed";
        return;
    }

#ifdef _WIN32
    unsigned long ul = 1;                  // 非零值表示启用非阻塞模式
    ioctlsocket(m_socketFD, FIONBIO, &ul); // 设置套接字为非阻塞模式
#elif defined(__linux__)
    // 获取文件描述符当前存在的标志
    int flags = fcntl(m_socketFD, F_GETFL, 0);
    // 将非阻塞添加进去,这样后面的连接操作不会等待连接完成,而是直接返回,避免阻塞其他操作
    fcntl(m_socketFD, F_SETFL, flags | O_NONBLOCK);
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
#ifdef _WIN32
    if (m_socketFD != INVALID_SOCKET)
    {
        closesocket(m_socketFD);
        m_socketFD = INVALID_SOCKET;
    }
#elif defined(__linux__)
    if (m_socketFD >= 0)
    {
        close(m_socketFD);
        m_socketFD = -1;
    }
#endif

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
    int attemptCount = 0;
    constexpr int maxAttempts = 10;

    while (m_isThreadRunning && !m_isConnected && attemptCount < maxAttempts)
    {
        attemptCount++;

        // 创建读集合和写集合，监听套接字的可读和可写状态
        fd_set rSet, wSet;

        // 清空集合
        FD_ZERO(&rSet);
        FD_ZERO(&wSet);

        // 设置超时时间为10秒
        struct timeval timeout = {10, 0};

        // 将socket加入到读写集合后
        // select函数监测套接字状态变化或超时
#ifdef _WIN32
        FD_SET(m_socketFD, &rSet);
        FD_SET(m_socketFD, &wSet);
        int retValue = select(0, &rSet, &wSet, nullptr, &timeout);
        if (retValue == SOCKET_ERROR)
        {
            int winError = WSAGetLastError();
            std::cerr << " (SOCKET_ERROR: " << winError << ")" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
#else
        FD_SET(m_socketFD, &rSet);
        FD_SET(m_socketFD, &wSet);
        // Linux下select第一个参数是监听的文件描述符的最大值加1
        int retValue = select(m_socketFD + 1, &rSet, &wSet, nullptr, &timeout);
        if (retValue == -1)
        {
            std::cerr << "select error: " << strerror(errno) << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
#endif
        // 不同平台select返回0时都表示超时
        else if (retValue == 0)
        {
            std::cerr << " (TIMEOUT)" << std::endl;
            continue;
        }

        // 根据写集合的状态判断连接是否成功
        // 读集合这里其实是多余的
        // 之后再用getsoctopt函数来获取具体的错误码，判断连接状态
        if (FD_ISSET(m_socketFD, &wSet))
        {
            int socketError = 0;
#ifdef _WIN32
            int len = sizeof(socketError);
            if (getsockopt(m_socketFD, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&socketError), &len) == 0 && socketError == 0)
#else
            socklen_t len = sizeof(socketError);
            if (getsockopt(m_socketFD, SOL_SOCKET, SO_ERROR, &socketError, &len) == 0 && socketError == 0)
#endif
            {
                std::cout << "socket connected successfully" << std::endl;
                m_isConnected = true;
                break;
            }
            else
            {
                // 现在连接失败之后是没有手动机制重连的
                // 实际项目中可以根据需要添加重连机制
                // 所以这里没有完全中断整个程序的其他线程
                std::cerr << "socket connection failed with error: " << socketError << std::endl;
            }
        }
    }
}

void VideoClient::doReceiveData()
{
    H264Decoder decoder;
    m_isReceiveThreadRunning = true;
    while (m_isThreadRunning)
    {
        // 10ms收取1次，这里不能接收太慢，如果频率太慢会导致大量socket数据丢弃，接收到的数据就是不连续的
        // 会导致解码不出来
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!m_isConnected)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // 先接收消息头
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
}

// 发送心跳包，告诉服务端，此客户端还活着
// 避免客户端非正常结束，服务端接收不到close信号
// 检测不到心跳包就直接关闭和此客户端的连接
void VideoClient::sendKeepAlivePacket()
{
    m_isKeepAliveThreadRunning = true;
    while (m_isThreadRunning)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // 没连接上的时候先空转
        if (!m_isConnected)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
}

bool VideoClient::receiveSocketData(std::vector<uint8_t> &buffer, size_t length)
{
    // 在Linux忽略SIGPIPE信号，防止在写入一个已经关闭的socket时程序被终止
#ifdef __linux__
    std::signal(SIGPIPE, SIG_IGN);
#endif

    buffer.resize(length);
    size_t receiveLength = 0;

    while (receiveLength < length)
    {
        /*
        接收数据
        第二个参数是从哪里开始存放数据的指针（Windows的recv要求缓冲区为char*）
        第三个参数是要接收的数据长度
        第四个参数是标志位，一般为0
        */
#ifdef _WIN32
        int nRet = recv(m_socketFD, reinterpret_cast<char *>(buffer.data() + receiveLength), static_cast<int>(length - receiveLength), 0);
        if (nRet == SOCKET_ERROR)
        {
            int errorCode = WSAGetLastError();
            // 处理非阻塞情况下没有数据可读的情况
            if (errorCode == WSAEWOULDBLOCK || errorCode == WSAEINTR)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
        }
        // 其他错误情况
        if (nRet < 0)
        {
            int errorCode = WSAGetLastError();
            std::cerr << "Windows socket receive error, code: " << errorCode << std::endl;
            return false;
        }
#else
        int nRet = recv(m_socketFD, buffer.data() + receiveLength, length - receiveLength, 0);
        if (nRet < 0)
        {
            if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            std::cerr << "socket receive error" << std::endl;
            return false;
        }
#endif
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
    // 在Linux忽略SIGPIPE信号，防止在写入一个已经关闭的socket时程序被终止
#ifdef __linux__
    std::signal(SIGPIPE, SIG_IGN);
#endif

    size_t sendLength = 0;

    while (sendLength < length)
    {
        /*
        发送数据
        第二个参数是要发送的数据指针（Windows的send要求缓冲区为const char*）
        第三个参数是要发送的数据长度
        第四个参数是标志位，一般为0
        */
#ifdef _WIN32
        int nRet = send(m_socketFD, reinterpret_cast<const char *>(buffer.data() + sendLength), static_cast<int>(length - sendLength), 0);
        if (nRet == SOCKET_ERROR)
        {
            int errorCode = WSAGetLastError();
            // 处理非阻塞情况下不能立即发送的情况
            if (errorCode == WSAEWOULDBLOCK || errorCode == WSAEINTR)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            std::cerr << "Windows socket send error, code: " << errorCode << std::endl;
            return false;
        }
#else
        int nRet = send(m_socketFD, buffer.data() + sendLength, length - sendLength, 0);
        if (nRet < 0)
        {
            if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            std::cerr << "socket send error" << std::endl;
            return false;
        }
#endif
        if (nRet == 0)
        {
            std::cerr << "connection close, socket send error" << std::endl;
            return false;
        }
        sendLength += nRet;
    }
    return true;
}

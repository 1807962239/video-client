#include "videoclient.h"

std::atomic_bool VideoClient::m_isThreadRunning = true;

VideoClient::VideoClient()
{
    std::signal(SIGINT, clientExitSignalProcess);
}

VideoClient::~VideoClient()
{
    closeSocketFD();
}

void VideoClient::startSocketConnection(const NetConnectInfo &netConnectInfo)
{
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

    // 将IPv4地址字符串转换为二进制地址缓冲区指针,存储在前面定义的协议的地址中
    if (inet_pton(AF_INET, netConnectInfo.m_serverIP.c_str(), &sockAddrIn.sin_addr.s_addr) < 0)
    {
        std::cerr << "convert IPv4 address string to binary buffer pointer failed";
        return;
    }

    // 获取文件描述符当前存在的标志
    int flags = fcntl(m_socketFD, F_GETFL, 0);
    // 将非阻塞添加进去,这样后面的连接操作不会等待连接完成,而是直接返回,避免阻塞其他操作
    fcntl(m_socketFD, F_SETFL, flags | O_NONBLOCK);

    connect(m_socketFD, reinterpret_cast<struct sockaddr *>(&sockAddrIn), sizeof(struct sockaddr));

    // 单独用一个线程来监听连接有没有完成并判断状态
    std::thread runWaitConnectionThread([this]()
                                        { this->doRunWaitConnection(); });
    runWaitConnectionThread.detach();

    // 再用一个线程接收服务端发来的实际的数据包
    std::thread receiveDataThread([this]()
                                  { this->doReceiveData(); });
    receiveDataThread.detach();

    // 发送心跳包，保持客户端运行，避免主线程直接运行完终止来不及执行子线程
    sendKeepAlivePacket();
}

void VideoClient::doRunWaitConnection()
{
    // 文件描述符集合类型
    // 分别创建读集合和写集合监听套接字的可读和可写状态
    fd_set rSet, wSet;

    // 清空集合
    FD_ZERO(&rSet);
    FD_ZERO(&wSet);

    // 将描述符放到集合中
    FD_SET(m_socketFD, &rSet);
    FD_SET(m_socketFD, &wSet);

    // 设置超时时间为10秒+0微秒
    struct timeval timeout = {10, 0};

    // 调用select函数等待套接字变成可读或可写状态
    // 第一个参数传入集合中的最大描述符+1,表示select需要检查范围的上限
    // 后面分别传入读集合和写集合来监听
    // 第四个参数传入nullptr表示不监视异常情况
    // 最后一个参数表示超时时间
    int retValue = select(m_socketFD + 1, &rSet, &wSet, nullptr, &timeout);

    if (retValue == -1)
    {
        // 错误
        std::cerr << "select called failed" << std::endl;
        return;
    }
    else if (retValue == 0)
    {
        // 超时
        std::cerr << "select is time out" << std::endl;
        return;
    }

    // 检查套接字是否在写集合中(连接完成的关键标志,不在直接return)
    if (!FD_ISSET(m_socketFD, &wSet))
    {
        std::cerr << "no write set" << std::endl;
        return;
    }

    // 获取套接字错误状态去进一步判断
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(m_socketFD, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
    {
        std::cerr << "getsockopt failed" << std::endl;
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
    while (m_isThreadRunning)
    {
        // 1s收取1次
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!m_isConnected)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        std::vector<u_int8_t> buffer(sizeof(NetMessageHeader));
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

        // 以下来测试收取到的数据
        std::cout << "收取到的视频数据大小：" << msgHeader.m_length << std::endl;
        // 打印数据的前十位
        std::cout << "数据前十位：";
        for (int i = 0; i < 10; i++)
        {
            std::cout << streamBuffer[i] << " ";
        }
        std::cout << std::endl;
    }
}

// 发送心跳包，告诉服务端，此客户端还活着
// 避免客户端非正常结束，服务端接收不到close信号
// 检测不到心跳包就直接关闭和此客户端的连接
void VideoClient::sendKeepAlivePacket()
{
    while (m_isThreadRunning)
    {
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

    std::cout << "stop send alive packet" << std::endl;
}

void VideoClient::clientExitSignalProcess(int num)
{
    m_isThreadRunning = false;
}

void VideoClient::closeSocketFD()
{
    if (m_socketFD >= 0)
    {
        close(m_socketFD);
        m_socketFD = -1;
    }
}

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
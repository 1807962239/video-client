#include "videoclient.h"

bool VideoClient::m_isThreadRunning = true;

VideoClient::VideoClient()
{
    signal(SIGINT, clientExitSignalProcess);
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
    sockAddrIn.sin_port = htons(netConnectInfo.getPort());

    // 将IPv4地址字符串转换为二进制地址缓冲区指针,存储在前面定义的协议的地址中
    if (inet_pton(AF_INET, netConnectInfo.getServerIp().c_str(), &sockAddrIn.sin_addr.s_addr) < 0)
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

    std::cout << "connect success" << std::endl;
}

// 发送心跳包，维持客户端运行
void VideoClient::sendKeepAlivePacket()
{
    while (m_isThreadRunning)
    {
        std::cout << "send alive packet..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
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
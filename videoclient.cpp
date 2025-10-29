#include "videoclient.h"

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
}
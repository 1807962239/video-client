#ifndef TYPE_H
#define TYPE_H

#include <string>

#define MSGHEADER_TYPE_KEEPALIVE 0
#define MSGHEADER_TYPE_CONTENT_VIDEO 1

// C++定义的结构体不需要加typedef也能直接调用

// 网络连接信息结构体
struct NetConnectInfo
{
    std::string m_serverIP = ""; // 服务端的IP
    int m_port = 0;              // 端口

    NetConnectInfo() = default;
    NetConnectInfo(const std::string &ip, int port)
        : m_serverIP(ip), m_port(port) {}
};

// 网络传输数据大小应该是一致的，避免因字节对齐问题导致数据大小不一致
#pragma pack(push, 1)

// 网络数据包的包头信息结构体
struct NetMessageHeader
{
    // 网络传输中推荐直接用字节数组
    // std::string可能会有问题
    char m_headerID[6];
    int m_msgType = 0;
    size_t m_length = 0;

    NetMessageHeader() = default;
    NetMessageHeader(const char *headerID, int msgType, size_t length)
        : m_msgType(msgType), m_length(length)
    {
        strncpy(m_headerID, headerID, sizeof(m_headerID) - 1);
        m_headerID[sizeof(m_headerID) - 1] = '\0';
    }
};

#pragma pack(pop)

#endif
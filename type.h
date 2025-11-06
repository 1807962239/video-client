#ifndef TYPE_H
#define TYPE_H

#include <string>

// 这两个用来判断数据是心跳包还是流媒体
#define MSGHEADER_TYPE_KEEPALIVE 0
#define MSGHEADER_TYPE_STREAM 1

// 这两个用来判断数据是视频流还是音频流
#define MSGHEADER_STREAM_VIDEO 3
#define MSGHEADER_STREAM_AUDIO 4

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
    uint16_t m_msgType;
    uint16_t m_subType;
    uint32_t m_length;

    NetMessageHeader() = default;
    NetMessageHeader(const char *headerID, uint16_t msgType, uint16_t subType, size_t length)
        : m_msgType(msgType), m_subType(subType), m_length(length)
    {
        strncpy(m_headerID, headerID, sizeof(m_headerID) - 1);
        m_headerID[sizeof(m_headerID) - 1] = '\0';
    }
};

#pragma pack(pop)

#endif
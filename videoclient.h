#ifndef VIDEOCLIENT_H
#define VIDEOCLIENT_H

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h> // 包含IPv4和IPv6地址的文本表示与二进制格式之间的转换的函数
#include <fcntl.h>
#include <sys/signal.h>
#include <cstring>

#include <iostream>
#include <string>
#include <thread>

class NetConnectInfo
{
private:
    std::string m_serverIP = "";
    int m_port = 0;

public:
    NetConnectInfo() = default;
    ~NetConnectInfo() = default;
    NetConnectInfo(const std::string &ip, int port)
        : m_serverIP(ip), m_port(port) {}
    const std::string &getServerIp() const { return m_serverIP; }
    int getPort() const { return m_port; }

    void setServerIp(const std::string &ip) { m_serverIP = ip; }
    void setPort(int port) { m_port = port; }
};

class VideoClient
{
public:
    VideoClient();
    ~VideoClient();

    void startSocketConnection(const NetConnectInfo &netConnectInfo);

private:
    void doRunWaitConnection();
    void sendKeepAlivePacket();
    static void clientExitSignalProcess(int num);
    void closeSocketFD();

private:
    int m_socketFD = -1;
    static bool m_isThreadRunning;
};

#endif
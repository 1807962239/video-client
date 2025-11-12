#ifndef VIDEOCLIENT_H
#define VIDEOCLIENT_H

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h> // 包含IPv4和IPv6地址的文本表示与二进制格式之间的转换的函数
#include <fcntl.h>
#include <csignal>
#include <cstring>

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>

#include "type.h"
#include "h264decoder.h"

using updateVideoCallback = std::function<void(YUVFrameData *yuvFrameData)>;

class VideoClient
{
public:
    VideoClient();
    ~VideoClient();

    void startSocketConnection(const NetConnectInfo &netConnectInfo);
    void stopSocketConnection();

    void setupUpdateVideoCallback(updateVideoCallback &&callback);

private:
    void doRunWaitConnection();
    void doReceiveData();
    void sendKeepAlivePacket();

    // 数据收发函数
    bool receiveSocketData(std::vector<uint8_t> &buffer, size_t length);
    bool sendSocketData(const std::vector<uint8_t> &buffer, size_t length);

private:
    int m_socketFD = -1;

    std::atomic_bool m_isThreadRunning = true;
    // 用两个标志来使线程优雅的退出
    std::atomic_bool m_isKeepAliveThreadRunning = false;
    std::atomic_bool m_isReceiveThreadRunning = false;

    bool m_isConnected = false;
    std::mutex m_receiveMutex;
    std::mutex m_sendMutex;

    updateVideoCallback m_updateVideoCallback;
};

#endif

#include "videoclient.h"

int main()
{
    // 这里设置的地址必须是服务端可用的IP地址,这样才能访问到特定主机的服务端
    // 通过ip addr show在服务端主机上查看其可用IP
    NetConnectInfo netConnectInfo("192.168.18.5", 30000);

    VideoClient videoClient;
    videoClient.startSocketConnection(netConnectInfo);
    return 0;
}
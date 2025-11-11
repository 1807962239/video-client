#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow{parent},
    m_pVideoClient(std::make_unique<VideoClient>())
{
    this->setFixedSize(640, 480);
    // 这里设置的地址必须是服务端可用的IP地址,这样才能访问到特定主机的服务端
    // 通过ip addr show在服务端主机上查看其可用IP
    NetConnectInfo netConnectInfo("192.168.18.5", 30000);

    m_pVideoClient->startSocketConnection(netConnectInfo);
}

MainWindow::~MainWindow()
{
    m_pVideoClient->stopSocketConnection();
}

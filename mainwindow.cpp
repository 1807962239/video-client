#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow{parent},
    m_pVideoClient(std::make_unique<VideoClient>())
{
    this->setFixedSize(640, 480);
    // 这里设置的地址必须是服务端可用的IP地址,这样才能访问到特定主机的服务端
    // 通过ip addr show在服务端主机上查看其可用IP
    NetConnectInfo netConnectInfo;
    if (!loadNetConfig(netConnectInfo)) {
        std::cerr << "load config failed, exit..." << std::endl;
        exit(0);
    }

    m_pVideoClient->startSocketConnection(netConnectInfo);

    auto updateVideoCallbackFunction = [this] (YUVFrameData *yuvFrameData) {
        if (yuvFrameData == nullptr) {
            return;
        }
        m_pOpenGLWidget->RendVideo(yuvFrameData);
    };
    m_pVideoClient->setupUpdateVideoCallback(updateVideoCallbackFunction);

    m_pOpenGLWidget = new OpenGLWidget(this);
    this->setCentralWidget(m_pOpenGLWidget);
}

MainWindow::~MainWindow()
{
    m_pVideoClient->stopSocketConnection();
}

bool MainWindow::loadNetConfig(NetConnectInfo &info)
{
    std::ifstream file("client_config");
    if (!file.is_open()) {
        std::cerr << "config file is not exist" << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        // 从0开始截取前pos个字符，刚好截取等号前面的
        std::string key = line.substr(0, pos);
        // 截取pos后面的部分
        std::string value = line.substr(pos + 1);
        if (key == "server_ip") {
            info.m_serverIP = value;
        } else if (key == "server_port") {
            info.m_port = std::stoi(value);
        }
    }
    file.close();
    return true;
}

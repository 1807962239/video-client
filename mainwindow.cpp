#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow{parent},
      m_pVideoClient(std::make_unique<VideoClient>())
{
    this->setFixedSize(640, 480 + 50); // 视频窗口大小为640x480，控制按钮区域高度为50
    // 这里设置的地址必须是服务端可用的IP地址,这样才能访问到特定主机的服务端
    // 通过ip addr show在服务端主机上查看其可用IP
    NetConnectInfo netConnectInfo;
    if (!loadNetConfig(netConnectInfo))
    {
        std::cerr << "load config failed, exit..." << std::endl;
        exit(0);
    }

    m_pVideoClient->startSocketConnection(netConnectInfo);

    auto updateVideoCallbackFunction = [this](YUVFrameData *yuvFrameData)
    {
        if (yuvFrameData == nullptr)
        {
            return;
        }
        m_pOpenGLWidget->RendVideo(yuvFrameData);
    };
    m_pVideoClient->setupUpdateVideoCallback(updateVideoCallbackFunction);

    initUi();
    connect(m_pRecordButton, &QPushButton::clicked, []()
            { VideoWriter::getInstance().toggleRecord(); });
}

MainWindow::~MainWindow()
{
    m_pVideoClient->stopSocketConnection();
}

void MainWindow::initUi()
{
    QWidget *centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    m_pOpenGLWidget = new OpenGLWidget(centralWidget);
    QWidget *controlWidget = new QWidget(centralWidget);
    mainLayout->addWidget(m_pOpenGLWidget);
    mainLayout->addWidget(controlWidget);

    QHBoxLayout *controlLayout = new QHBoxLayout(controlWidget);
    controlWidget->setFixedHeight(50); // 设置控制按钮区域的高度为50
    m_pRecordButton = new QPushButton("Record", controlWidget);
    controlLayout->addItem(new QSpacerItem(40, 50, QSizePolicy::Expanding, QSizePolicy::Minimum));
    controlLayout->addWidget(m_pRecordButton);
    controlLayout->addItem(new QSpacerItem(40, 50, QSizePolicy::Expanding, QSizePolicy::Minimum));
}

bool MainWindow::loadNetConfig(NetConnectInfo &info)
{
    std::ifstream file("client_config.txt");
    if (!file.is_open())
    {
        std::cerr << "config file is not exist" << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line))
    {
        size_t pos = line.find('=');
        if (pos == std::string::npos)
        {
            continue;
        }
        // 从0开始截取前pos个字符，刚好截取等号前面的
        std::string key = line.substr(0, pos);
        // 截取pos后面的部分
        std::string value = line.substr(pos + 1);
        if (key == "server_ip")
        {
            info.m_serverIP = value;
        }
        else if (key == "server_port")
        {
            info.m_port = std::stoi(value);
        }
    }
    file.close();
    return true;
}

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include <fstream>

#include "videoclient.h"
#include "openglwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    bool loadNetConfig(NetConnectInfo &info);

private:
    std::unique_ptr<VideoClient> m_pVideoClient;
    OpenGLWidget *m_pOpenGLWidget = nullptr;

signals:
};

#endif // MAINWINDOW_H

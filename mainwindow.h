#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>

#include "videoclient.h"
#include "openglwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    std::unique_ptr<VideoClient> m_pVideoClient;
    OpenGLWidget *m_pOpenGLWidget = nullptr;

signals:
};

#endif // MAINWINDOW_H

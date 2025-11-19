#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#include <QMatrix4x4>
#include <QOpenGLFunctions>

#include "type.h"

struct Vertex
{
    float x, y, z;
    float u, v;
};

class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    OpenGLWidget(QWidget *parent = nullptr);
    ~OpenGLWidget();

    // 根据传过来的YUV数据执行渲染
    void RendVideo(YUVFrameData *frame);

private:
    void initializeGLSLShaders();
    GLuint createImageTextures(QString &pathString);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    QOpenGLShaderProgram *m_pShaderProgram = nullptr;
    GLuint m_textures[3];

    // YUV每个分量的数据长度
    int m_yFrameLength = 0;
    int m_uFrameLength = 0;
    int m_vFrameLength = 0;

    int m_videoWidth = 0;
    int m_videoHeight = 0;

    // 数据缓冲区
    unsigned char *m_pBufYuv420p = nullptr;

    bool m_glewInitSuccessfully = false;
};

#endif // OPENGLWIDGET_H

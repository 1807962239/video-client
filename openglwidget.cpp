#include "openglwidget.h"

OpenGLWidget::OpenGLWidget(QWidget *parent)
    : QOpenGLWidget{parent}
{
}

OpenGLWidget::~OpenGLWidget()
{
    if (m_pBufYuv420p != nullptr)
    {
        free(m_pBufYuv420p);
        m_pBufYuv420p = nullptr;
    }

    glDeleteTextures(3, m_textures);
}

void OpenGLWidget::RendVideo(YUVFrameData *yuvFrame)
{

    if (yuvFrame == nullptr)
    {
        return;
    }

    // 传入数据宽度与当前宽高不匹配则释放缓冲区并在后续重新分配缓冲区
    if (m_videoHeight != (int)yuvFrame->m_height || m_videoWidth != (int)yuvFrame->m_width)
    {
        if (m_pBufYuv420p != nullptr)
        {
            free(m_pBufYuv420p);
            m_pBufYuv420p = nullptr;
        }
    }

    // 根据传入数据设置宽高
    m_videoWidth = yuvFrame->m_width;
    m_videoHeight = yuvFrame->m_height;

    // 根据传入数据获取yuv每个分量的数据长度
    m_yFrameLength = yuvFrame->m_luma.m_length;
    m_uFrameLength = yuvFrame->m_chromaB.m_length;
    m_vFrameLength = yuvFrame->m_chromaR.m_length;

    // 计算存当前一帧数据的长度，由yuv每个分量的数据长度相加得出
    int nLen = m_yFrameLength + m_uFrameLength + m_vFrameLength;

    // 根据数据长度分配内存，每个数据大小是一字节
    if (m_pBufYuv420p == nullptr)
    {
        m_pBufYuv420p = (unsigned char *)malloc(nLen);
    }

    /*
     * 拷贝的目标位置是m_pBufYuv420p的起始位置
     * 拷贝的源数据位置是结构体yuvFrame的luma的dataBuffer的位置
     * 大小是y分量的数据长度
     */
    memcpy(m_pBufYuv420p, yuvFrame->m_luma.m_dataBuffer.data(), m_yFrameLength);
    memcpy(m_pBufYuv420p + m_yFrameLength, yuvFrame->m_chromaB.m_dataBuffer.data(), m_uFrameLength);
    memcpy(m_pBufYuv420p + m_yFrameLength + m_uFrameLength, yuvFrame->m_chromaR.m_dataBuffer.data(), m_vFrameLength);

    update();
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0f);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glGenTextures(3, m_textures);

    initializeGLSLShaders();
}

void OpenGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    static Vertex triangleVert[] = {
        {-1, 1, 1, 0, 0},
        {-1, -1, 1, 0, 1},
        {1, 1, 1, 1, 0},
        {1, -1, 1, 1, 1}};

    QMatrix4x4 matrix;
    // 设置正交变换，也就是视角，使绘制内容铺满视角
    matrix.ortho(-1, 1, -1, 1, 0.1, 1000);
    // 将绘制内容向z的负方向移动
    matrix.translate(0, 0, -3);

    // 将此着色器程序绑定到当前上下文环境
    m_pShaderProgram->bind();

    // 将矩阵传入
    m_pShaderProgram->setUniformValue("uni_mat", matrix);

    // 传入顶点和uv坐标
    m_pShaderProgram->enableAttributeArray("attr_position");
    m_pShaderProgram->enableAttributeArray("attr_uv");
    m_pShaderProgram->setAttributeArray("attr_position", GL_FLOAT, triangleVert, 3, sizeof(Vertex));
    m_pShaderProgram->setAttributeArray("attr_uv", GL_FLOAT, &triangleVert[0].u, 2, sizeof(Vertex));

    // Y分量纹理的纹理采样器的纹理单元为0
    m_pShaderProgram->setUniformValue("uni_textureY", 0);
    // 激活纹理单元0
    glActiveTexture(GL_TEXTURE0);
    // 绑定当前纹理
    glBindTexture(GL_TEXTURE_2D, m_textures[0]);
    // 设定默认像素对齐为1
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // GL_LUMINANCE表明传入的数据格式为单通道亮度（传YUV某个分量时使用这个）
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_videoWidth, m_videoHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_pBufYuv420p);
    // 恢复默认像素对齐为4
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    m_pShaderProgram->setUniformValue("uni_textureU", 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_textures[1]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_videoWidth / 2, m_videoHeight / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, (char *)(m_pBufYuv420p + m_yFrameLength));
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    m_pShaderProgram->setUniformValue("uni_textureV", 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_textures[2]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_videoWidth / 2, m_videoHeight / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, (char *)(m_pBufYuv420p + m_yFrameLength + m_uFrameLength));
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 绘制
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_pShaderProgram->disableAttributeArray("attr_position");
    m_pShaderProgram->disableAttributeArray("attr_uv");

    m_pShaderProgram->release();
}

void OpenGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void OpenGLWidget::initializeGLSLShaders()
{
    // 初始化顶点着色器
    QOpenGLShader *vertexShader = new QOpenGLShader(QOpenGLShader::Vertex, this);
    bool bCompileVS = vertexShader->compileSourceFile(":/shaders/vertex.vert");
    if (bCompileVS == false)
    {
        qDebug() << "VS Compile ERROR:" << vertexShader->log();
    }

    // 初始化片段着色器
    QOpenGLShader *fragmentShader = new QOpenGLShader(QOpenGLShader::Fragment, this);
    bool bCompileFS = fragmentShader->compileSourceFile(":/shaders/fragment.frag");
    if (bCompileFS == false)
    {
        qDebug() << "FS Compile ERROR:" << fragmentShader->log();
    }

    // 初始化shader程序并链接
    m_pShaderProgram = new QOpenGLShaderProgram(this);
    m_pShaderProgram->addShader(vertexShader);
    m_pShaderProgram->addShader(fragmentShader);
    bool linkStatus = m_pShaderProgram->link();

    if (linkStatus == false)
    {
        qDebug() << "LINK ERROR:" << m_pShaderProgram->log();
    }

    if (vertexShader != nullptr)
    {
        delete vertexShader;
        vertexShader = nullptr;
    }

    if (fragmentShader != nullptr)
    {
        delete fragmentShader;
        fragmentShader = nullptr;
    }
}

// 没有用到这段代码，单纯用来比对用rgb图片与yuv某个分量初始化纹理的区别
GLuint OpenGLWidget::createImageTextures(QString &pathString)
{
    GLuint textureId;
    glGenTextures(1, &textureId);            // 产生纹理索引
    glBindTexture(GL_TEXTURE_2D, textureId); // 绑定纹理索引，之后的操作都针对当前纹理索引

    QImage texImage = QImage(pathString.toLocal8Bit().data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 指当纹理图象被使用到一个大于它的形状上时
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // 指当纹理图象被使用到一个小于或等于它的形状上时
    // 把纹理上传到OpenGL
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texImage.width(), texImage.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, texImage.rgbSwapped().bits()); // 指定参数，生成纹理

    return textureId;
}

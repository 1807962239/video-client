#include "h264decoder.h"

H264Decoder::H264Decoder()
{
    initCodec();
}

H264Decoder::~H264Decoder()
{
    if (m_pCodecContext != nullptr)
    {
        avcodec_free_context(&m_pCodecContext);
        m_pCodecContext = nullptr;
    }

    if (m_pVideoFrame != nullptr)
    {
        av_frame_free(&m_pVideoFrame);
        m_pVideoFrame = nullptr;
    }
}

void H264Decoder::initCodec()
{
    m_pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (m_pCodec == nullptr)
    {
        std::cerr << "avcodec_find_decoder error" << std::endl;
    }

    m_pCodecContext = avcodec_alloc_context3(m_pCodec);
    if (m_pCodecContext == nullptr)
    {
        std::cerr << "avcode_alloc_context3 error" << std::endl;
    }

    if (avcodec_open2(m_pCodecContext, m_pCodec, nullptr) < 0)
    {
        std::cerr << "avcodec_open2 error" << std::endl;
    }

    m_pVideoFrame = av_frame_alloc();
    if (m_pVideoFrame == nullptr)
    {
        std::cerr << "av_frame_alloc error" << std::endl;
    }
}

void H264Decoder::copyFrameData(uint8_t *src, uint8_t *dst, int linesize, int width, int height)
{
    for (int i = 0; i < height; i++)
    {
        memcpy(dst, src, width);

        dst += width;

        src += linesize;
    }
}

int H264Decoder::decodeH264Packet(std::vector<uint8_t> &&buffer, size_t length, YUVFrameData *outFrame)
{
    if (outFrame == nullptr)
    {
        std::cerr << "Output frame is null" << std::endl;
        return -1;
    }

    AVPacket *pkg = av_packet_alloc();
    if (pkg == nullptr)
    {
        std::cerr << "Failed to allocate packet" << std::endl;
    }
    pkg->data = buffer.data();
    pkg->size = length;

    int ret = 0;
    ret = avcodec_send_packet(m_pCodecContext, pkg);
    av_packet_free(&pkg);
    if (ret != 0)
    {
        std::cerr << "Error sending packet to decoder: " << ret << std::endl;
        return -1;
    }

    ret = avcodec_receive_frame(m_pCodecContext, m_pVideoFrame);
    if (ret != 0)
    {
        std::cerr << "Error receiving frame from decoder: " << ret << std::endl;
        return -1;
    }

    size_t lumaLength = m_pCodecContext->width * m_pCodecContext->height;
    size_t chromaBLength = m_pCodecContext->width / 2 * m_pCodecContext->height / 2;
    size_t chromaRLength = m_pCodecContext->width / 2 * m_pCodecContext->height / 2;

    outFrame->m_luma.m_dataBuffer.resize(lumaLength);
    outFrame->m_chromaB.m_dataBuffer.resize(chromaBLength);
    outFrame->m_chromaR.m_dataBuffer.resize(chromaRLength);

    outFrame->m_luma.m_length = lumaLength;
    outFrame->m_chromaB.m_length = chromaBLength;
    outFrame->m_chromaR.m_length = chromaRLength;

    outFrame->m_width = m_pCodecContext->width;
    outFrame->m_height = m_pCodecContext->height;

    copyFrameData(m_pVideoFrame->data[0], outFrame->m_luma.m_dataBuffer.data(), m_pVideoFrame->linesize[0], m_pCodecContext->width, m_pCodecContext->height);
    copyFrameData(m_pVideoFrame->data[1], outFrame->m_chromaB.m_dataBuffer.data(), m_pVideoFrame->linesize[1], m_pCodecContext->width / 2, m_pCodecContext->height / 2);
    copyFrameData(m_pVideoFrame->data[2], outFrame->m_chromaR.m_dataBuffer.data(), m_pVideoFrame->linesize[2], m_pCodecContext->width / 2, m_pCodecContext->height / 2);

    return 0;
}

#ifndef H264DECODER_H
#define H264DECODER_H

#include "type.h"

#include <iostream>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class H264Decoder
{
public:
    H264Decoder();
    ~H264Decoder();

    int decodeH264Packet(std::vector<uint8_t> &&buffer, size_t length, YUVFrameData *outBuffer);

private:
    void initCodec();
    void copyFrameData(uint8_t *src, uint8_t *dst, int linesize, int width, int height);

private:
    const AVCodec *m_pCodec = nullptr;
    AVCodecContext *m_pCodecContext = nullptr;
    AVFrame *m_pVideoFrame = nullptr;
};

#endif // H264DECODER_H

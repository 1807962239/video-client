
#ifndef VIDEOWRITER_H
#define VIDEOWRITER_H

#include <iostream>
#include <array>
#include <vector>
#include <algorithm>

#include <chrono>
#include <ratio>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
}

#define AV_MAX_AUDIO_DATA_SIZE 1280
#define AV_MAX_VIDEO_DATA_SIZE 64 * 1024

// H.264 SPS/PPS参数缓存结构体
struct AvccBox
{
    size_t spsLength = 0;                 // SPS数据长度
    size_t ppsLength = 0;                 // PPS数据长度
    std::array<uint8_t, 128> spsBuffer{}; // SPS数据缓存
    std::array<uint8_t, 64> ppsBuffer{};  // PPS数据缓存
};

// H.264 NAL单元类型枚举
enum H264NalType
{
    Nal = 0,      // 未知NAL
    Slice = 1,    // 普通帧
    SliceDpa = 2, // DPA帧
    SliceDpb = 3, // DPB帧
    SliceDpc = 4, // DPC帧
    SliceIdr = 5, // IDR关键帧
    Sei = 6,      // SEI信息帧
    Sps = 7,      // SPS参数帧
    Pps = 8       // PPS参数帧
};

// H.264 NALU单元结构体
struct H264Nalu
{
    H264NalType type = Nal;    // NALU类型
    size_t size = 0;           // 数据长度（字节）
    std::vector<uint8_t> data; // 数据
};

// 视频帧数据结构体
struct VideoFrame
{
    int codecId = 0;                                    // 编码器ID（如AV_CODEC_ID_H264）
    size_t size = 0;                                    // 数据长度（字节）
    std::array<uint8_t, AV_MAX_VIDEO_DATA_SIZE> data{}; // 视频数据缓存
};

// 音频帧数据结构体
struct AudioFrame
{
    int codecId = 0;                                    // 编码器ID（如AV_CODEC_ID_AAC）
    size_t size = 0;                                    // 数据长度（字节）
    std::array<uint8_t, AV_MAX_AUDIO_DATA_SIZE> data{}; // 音频数据缓存
};

class VideoWriter
{
public:
    static VideoWriter &getInstance();

    // 控制接口
    void start(const std::string &filePath);
    void stop();
    void toggleRecord();
    bool getStatus() const
    {
        return m_recordStatus;
    }
    void writeVideoData(std::vector<uint8_t> buffer, size_t length);
    void writeAudioData(std::vector<uint8_t> buffer, size_t length);

private:
    VideoWriter();
    ~VideoWriter();

    void setVideoSize(int width, int height);

    // FFmpeg相关
    void initVideoWriter();
    void resetAvDataInfo(bool keepCodecConfig = false);
    void writeVideoHeader();
    void initVideoStreamInfo();
    void initAudioStreamInfo();

    // H264相关
    bool readOneNaluFromBuff(const std::vector<uint8_t> &buffer, size_t bufferSize, size_t &offset, H264Nalu &nalu);
    void writeVideoFrame(const VideoFrame &frame, int keyFlags);
    // AAC相关
    void writeAudioStream(const AudioFrame &frame);

    // AudioSpecificConfig相关
    void getIndexConfigure(unsigned int sample, unsigned int channels, std::array<uint8_t, 2> &indexBuff);
    int getSampleIndex(unsigned int sample);

private:
    // 编解码器相关
    AVFormatContext *m_pFormatContex = nullptr;
    AVStream *m_pVideoStream = nullptr;
    AVStream *m_pAudioStream = nullptr;

    bool m_recordStatus = false;      // 整体状态
    bool m_startRecordStatus = false; // 视频帧开始写入状态

    // 视频基础信息
    int m_videoWidth = 0;
    int m_videoHeight = 0;
    std::string m_fileString = "";

    // 时间戳
    // pts的单位就是以时间基为单位，表示当前帧在第几个时间基单位上
    // 而视频的时间基就是1/帧率，音频的时间基就是1/采样率
    // 所以视频帧的pts就是以帧为单位递增，音频帧的pts就是以采样数为单位递增
    int m_videoFrameCount = 0; // 已写入的视频帧数
    int m_audioFrameCount = 0; // 已写入的音频帧数

    AvccBox m_avcCBox = {}; // H.264 SPS/PPS数据
    bool m_spsPpsReady = false;
};

#endif
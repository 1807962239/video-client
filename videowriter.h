
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
// #include <libavdevice/avdevice.h>
#include <libavutil/avutil.h>
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

// H.264 NALU单元结构体
struct H264Nalu
{
    H264NalType type = Nal;    // NALU类型
    size_t size = 0;           // 数据长度（字节）
    std::vector<uint8_t> data; // 数据
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

// 视频帧数据结构体
struct VideoFrame
{
    int codecId = 0;                                    // 编码器ID（如AV_CODEC_ID_H264）
    uint64_t timeStamp = 0;                             // 时间戳（毫秒）
    size_t size = 0;                                    // 数据长度（字节）
    std::array<uint8_t, AV_MAX_VIDEO_DATA_SIZE> data{}; // 视频数据缓存
};

// 音频帧数据结构体
struct AudioFrame
{
    int codecId = 0;                                    // 编码器ID（如AV_CODEC_ID_AAC）
    uint64_t timeStamp = 0;                             // 时间戳（毫秒）
    size_t size = 0;                                    // 数据长度（字节）
    std::array<uint8_t, AV_MAX_AUDIO_DATA_SIZE> data{}; // 音频数据缓存
};

// 写入流的帧时间戳状态（用于PTS/DTS计算）
struct StreamWriteState
{
    int currentPts = 0;              // 当前帧的PTS（容器时间基单位）
    double currentTimeSec = 0.0;     // 当前帧的显示时间（秒）
    uint64_t totalElapsedMs = 0;     // 累计写入时长（毫秒）
    uint64_t lastInputTimestamp = 0; // 上一帧输入的原始时间戳（毫秒）
};

class VideoWriter
{
public:
    static VideoWriter &getInstance();

    // 控制接口
    void start(const std::string &filePath);
    void stop();
    void setVideoSize(int width, int height);
    void writeVideoData(std::vector<uint8_t> buffer, size_t length);
    void writeAudioData(std::vector<uint8_t> buffer, size_t length);

private:
    VideoWriter();
    ~VideoWriter();

    // FFmpeg相关
    void initVideoWriter();
    void resetAvDataInfo();
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

    // 时间戳辅助函数
    uint64_t elapseMs() const;

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

    // 时间戳状态
    StreamWriteState m_videoWriteState = {};
    StreamWriteState m_audioWriteState = {};

    AvccBox m_avcCBox = {}; // H.264 SPS/PPS数据
    bool m_spsPpsReady = false;

    std::chrono::system_clock::time_point m_startTimeStamp;
    bool m_startTimeStampSet = false; // 标记起始时间戳是否已设置

    int m_fileTotalSize = 0;
};

#endif
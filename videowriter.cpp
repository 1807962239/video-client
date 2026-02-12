#include "videowriter.h"

VideoWriter &VideoWriter::getInstance()
{
    static VideoWriter instance;
    return instance;
}

VideoWriter::VideoWriter() {}

VideoWriter::~VideoWriter()
{
    stop();
}

void VideoWriter::start(const std::string &filePath)
{
    m_fileString = filePath;
    m_recordStatus = true;

    setVideoSize(640, 480);
}

void VideoWriter::stop()
{
    m_recordStatus = false;

    // 写入文件尾部（trailer）
    if (m_pFormatContex)
    {

        int ret = av_write_trailer(m_pFormatContex);
        if (ret != 0)
        {
            std::cerr << "Call av_write_trailer function failed, return:" << ret << std::endl;
        }
    }

    // 释放视频流
    m_pVideoStream = nullptr;
    // 释放音频流
    m_pAudioStream = nullptr;

    // 关闭文件IO，第二个判断条件是输出格式不包含AVFMT_NOFILE标志，表示需要手动关闭文件IO
    if (m_pFormatContex && m_pFormatContex->pb && !(m_pFormatContex->oformat->flags & AVFMT_NOFILE))
    {
        int ret = avio_close(m_pFormatContex->pb);
        if (ret != 0)
        {
            std::cerr << "Call avio_close function failed, return:" << ret << std::endl;
        }
        m_pFormatContex->pb = nullptr;
    }

    // 释放格式上下文
    if (m_pFormatContex)
    {
        avformat_free_context(m_pFormatContex);
        m_pFormatContex = nullptr;
    }

    // 重置信息，包括起始时间戳
    resetAvDataInfo();
}

void VideoWriter::toggleRecord()
{
    if (m_recordStatus)
    {
        stop();
    }
    else
    {
        start("output.mp4");
    }
}

void VideoWriter::initVideoWriter()
{
    if (m_pFormatContex == nullptr)
    {
        m_pFormatContex = avformat_alloc_context();
        m_pFormatContex->oformat = av_guess_format("mp4", nullptr, nullptr);
    }

    if (m_pVideoStream == nullptr)
    {
        initVideoStreamInfo();
    }
    if (m_pAudioStream == nullptr)
    {
        // initAudioStreamInfo();
    }

    // 重新写入时要重置信息（保留SPS/PPS与avcC数据）
    resetAvDataInfo(true);
    writeVideoHeader();
}

void VideoWriter::initVideoStreamInfo()
{
    // 创建视频流
    // 第二个参数可传入编码器指针，这里传入nullptr由FFmpeg自动选择合适编码器
    m_pVideoStream = avformat_new_stream(m_pFormatContex, nullptr);

    if (m_pVideoStream == nullptr)
    {
        return;
    }

    // 流的id通常设置为当前流的数量减一（刚插入的流）
    m_pVideoStream->id = m_pFormatContex->nb_streams - 1;

    // AVCodecParameters是存储编解码相关信息的结构体
    AVCodecParameters *pCodecPar = m_pVideoStream->codecpar;
    pCodecPar->codec_id = AV_CODEC_ID_H264;     // 编码格式为H264
    pCodecPar->codec_type = AVMEDIA_TYPE_VIDEO; // 媒体类型为视频

    // 设置视频流的时间基准和其他基本参数
    m_pVideoStream->time_base = (AVRational){1, 25}; // 视频流的时间基表示每秒25帧
    pCodecPar->width = m_videoWidth;
    pCodecPar->height = m_videoHeight;
    pCodecPar->format = AV_PIX_FMT_YUV420P;

    // 存在SPS和PPS数据时，构建avcC数据放入extradata中
    if (m_avcCBox.ppsLength > 0 && m_avcCBox.spsLength > 0)
    {
        size_t spsLen = m_avcCBox.spsLength;
        size_t ppsLen = m_avcCBox.ppsLength;

        uint8_t profile = 0x64; // profile初始化为100，表示高质量baseline profile
        uint8_t compat = 0x00;  // 兼容性初始化为0，表示没有兼容性
        uint8_t level = 0x28;   // level初始化为40，表示码流复杂度和性能在level 4.0
        // 从SPS数据中提取profile、compatibility和level信息，并覆盖默认值
        if (spsLen >= 4)
        {
            profile = m_avcCBox.spsBuffer[1];
            compat = m_avcCBox.spsBuffer[2];
            level = m_avcCBox.spsBuffer[3];
        }

        /*
        6: AVCC头部固定长度
        2: 存储SPS的长度的字段长度
        spsLen: SPS数据长度
        1: 存储PPS数量的字段长度，这里固定为1，SPS的数量字段隐藏在AVCC头部
        2: 存储PPS长度的字段长度
        ppsLen: PPS数据长度
        */
        const size_t avccLen = 6 + 2 + spsLen + 1 + 2 + ppsLen;
        // 栈上分配avcc数据内存，额外加上AV_INPUT_BUFFER_PADDING_SIZE字节以防止溢出
        std::vector<uint8_t> avcc(avccLen + AV_INPUT_BUFFER_PADDING_SIZE, 0);
        size_t offset = 0;     // 索引偏移量
        avcc[offset++] = 0x01; // avcc版本号，固定为1

        // 写入H.264的profile、compatibility、level，分别对应SPS的第2、3、4字节
        avcc[offset++] = profile;
        avcc[offset++] = compat;
        avcc[offset++] = level;

        /*
        这个字节位置代表的参数是lengthSizeMinusOne，低两位表示NALU长度字段的长度减一
        常见的NALU长度字段长度为4字节，所以低两位设置为11，代表十进制3，也就是4-1=3
        高6位保留，设置为111111，所以是0xFF
        */
        avcc[offset++] = 0xFF;

        /*
        这个字节位置代表的参数是numOfSequenceParameterSets，低5位表示SPS的数量
        SPS数量通常为1，所以这里低五位设置为00001，代表十进制1
        高3位保留，设置为111，所以是0xE1
        */
        avcc[offset++] = 0xE1;

        /*
        这两位存储SPS的长度
        采用大端存储方式，第一个字节存储高8位，第二个字节存储低8位
        */
        avcc[offset++] = (spsLen >> 8) & 0xFF; // 右移8位，把高8位移到了低8位的位置，再与0xFF做与运算得到此时的低8位
        avcc[offset++] = spsLen & 0xFF;

        // 写入SPS数据
        std::copy_n(m_avcCBox.spsBuffer.begin(), spsLen, avcc.begin() + offset);
        offset += spsLen;

        // PPS数量，通常为1，和SPS不一样，这里直接写入1
        avcc[offset++] = 0x01;
        // 这两位存储PPS的长度，采用大端存储方式，和SPS长度存储方式一样
        avcc[offset++] = (ppsLen >> 8) & 0xFF;
        avcc[offset++] = ppsLen & 0xFF;
        // 写入PPS数据
        std::copy_n(m_avcCBox.ppsBuffer.begin(), ppsLen, avcc.begin() + offset);
        offset += ppsLen;

        // 填充多余的字节为0（vector已初始化为0）

        // 将构建好的avcC数据放入extradata中
        pCodecPar->extradata = (uint8_t *)av_malloc(avccLen + AV_INPUT_BUFFER_PADDING_SIZE);
        memcpy(pCodecPar->extradata, avcc.data(), avccLen + AV_INPUT_BUFFER_PADDING_SIZE);
        pCodecPar->extradata_size = avccLen;
    }
}

void VideoWriter::initAudioStreamInfo()
{
    // 创建音频流
    // 第二个参数可传入编码器指针，这里传入0由FFmpeg自动选择合适编码器
    m_pAudioStream = avformat_new_stream(m_pFormatContex, 0);
    if (m_pAudioStream == nullptr)
    {
        return;
    }

    // 流的id通常设置为当前流的数量减一（刚插入的流）
    m_pAudioStream->id = m_pFormatContex->nb_streams - 1;

    // 设置音频流的时间基准和其他基本参数
    m_pAudioStream->time_base = (AVRational){1, 8000};
    m_pAudioStream->discard = AVDISCARD_NONE; // 处理流时不丢弃所有数据包

    // AVCodecParameters是存储编解码相关信息的结构体
    AVCodecParameters *pCodecPar = m_pAudioStream->codecpar;
    pCodecPar->codec_id = AV_CODEC_ID_AAC;               // 编码格式为AAC
    pCodecPar->codec_type = AVMEDIA_TYPE_AUDIO;          // 媒体类型为音频
    pCodecPar->bit_rate = 8000;                          // 比特率
    pCodecPar->sample_rate = 8000;                       // 采样率
    pCodecPar->format = AV_SAMPLE_FMT_S16;               // 采样格式
    av_channel_layout_default(&pCodecPar->ch_layout, 1); // 声道布局为单声道

    /*
    构建AAC的AudioSpecificConfig数据放入extradata中
    只有两个字节，具体两个字节存储内容在getIndexConfigure函数中实现
    */
    std::array<uint8_t, 2> indexBuffer = {0};
    unsigned int sampleIndex = getSampleIndex(8000);
    getIndexConfigure(sampleIndex, 1, indexBuffer);

    // 用vector构建extradata buffer，自动填充padding为0
    const size_t ascLen = indexBuffer.size();
    std::vector<uint8_t> asc(ascLen + AV_INPUT_BUFFER_PADDING_SIZE, 0);
    std::copy_n(indexBuffer.begin(), ascLen, asc.begin());

    // 分配extradata并拷贝
    pCodecPar->extradata = (uint8_t *)av_malloc(ascLen + AV_INPUT_BUFFER_PADDING_SIZE);
    memcpy(pCodecPar->extradata, asc.data(), ascLen + AV_INPUT_BUFFER_PADDING_SIZE);
    pCodecPar->extradata_size = ascLen;
}

// 构建AAC的AudioSpecificConfig数据
void VideoWriter::getIndexConfigure(unsigned int sample, unsigned int channels, std::array<uint8_t, 2> &indexBuff)
{
    // 第1字节： 5位objectType + 3位采样率索引低4位里的高3位
    // 第2字节： 采样率索引的低1位 + 4位声道数 + 1位填充0

    unsigned int objectType = 2; // 2代表类型为AAC LC

    // 采样率索引低4位里的高3位（bit3~bit1）
    unsigned char sampleIndexHigh = (sample >> 1) & 0x07;
    // 采样率索引低1位（bit0）
    unsigned char sampleIndexLow = sample & 0x01;

    // 第1字节：objectType(5位) | sampleIndex高3位
    indexBuff[0] = (objectType << 3) | sampleIndexHigh;
    // 第2字节：sampleIndex低1位(1位) | 声道数(4位) | 填充0(3位)
    // 采样率索引低1位应占bit7，声道数占bit6~bit3
    indexBuff[1] = ((sample & 0x01) << 7) | ((channels & 0x0F) << 3);
}

// AAC规定的AudioSpecificConfig采样率索引
int VideoWriter::getSampleIndex(unsigned int samples)
{
    switch (samples)
    {
    case 96000:
        return 0;
    case 88200:
        return 1;
    case 64000:
        return 2;
    case 48000:
        return 3;
    case 44100:
        return 4;
    case 32000:
        return 5;
    case 24000:
        return 6;
    case 22050:
        return 7;
    case 16000:
        return 8;
    case 12000:
        return 9;
    case 11025:
        return 10;
    case 8000:
        return 11;
    case 7350:
        return 12;
    default:
        return 0;
    }
}

// 写入容器头部信息
void VideoWriter::writeVideoHeader()
{
    const char *movieUrl = m_fileString.c_str();

    /*
    打开输出文件
    第一个参数是AVIOContext的二级指针，AVIOContext是FFmpeg中封装的文件IO操作结构体
    第三个参数是操作标志，这里是写操作
    */
    if (avio_open(&m_pFormatContex->pb, movieUrl, AVIO_FLAG_WRITE) != 0)
    {
        m_recordStatus = false;
        std::cerr << "Call avio_open function failed." << std::endl;
        return;
    }

    // 写入头部信息
    if (avformat_write_header(m_pFormatContex, nullptr) < 0)
    {
        m_recordStatus = false;
        std::cerr << "Call avformat_write_header function failed." << std::endl;
        return;
    }
}

void VideoWriter::resetAvDataInfo(bool keepCodecConfig)
{
    m_startRecordStatus = false;
    m_videoFrameCount = 0;
    m_audioFrameCount = 0;
    if (!keepCodecConfig)
    {
        m_avcCBox = AvccBox{};
        m_spsPpsReady = false;
    }
}

void VideoWriter::setVideoSize(int width, int height)
{
    m_videoWidth = width;
    m_videoHeight = height;
}

// 将现成的H.264数据写入视频流
void VideoWriter::writeVideoData(std::vector<uint8_t> buffer, size_t length)
{
    if (!m_recordStatus)
    {
        return;
    }

    // H.264数据小于4字节无法构成NALU单元
    if (buffer.empty() || length < 4)
    {
        return;
    }

    H264Nalu naluUnit;
    size_t naluPos = 0;

    // 解析NALU单元
    while (readOneNaluFromBuff(buffer, buffer.size(), naluPos, naluUnit))
    {
        if (naluUnit.type == Sps)
        {
            std::cout << "NALU: SPS:" << std::endl;
            if (naluUnit.size > 0)
            {
                std::copy(naluUnit.data.begin(), naluUnit.data.end(), m_avcCBox.spsBuffer.begin());
                m_avcCBox.spsLength = naluUnit.size;
            }
            // SPS数据读取完毕，只有在SPS+PPS都就绪时才接受后续的I帧和P帧数据
            m_spsPpsReady = (m_avcCBox.spsLength > 0 && m_avcCBox.ppsLength > 0);
        }
        else if (naluUnit.type == Pps)
        {
            std::cout << "NALU: PPS:" << std::endl;
            if (naluUnit.size > 0)
            {
                std::copy(naluUnit.data.begin(), naluUnit.data.end(), m_avcCBox.ppsBuffer.begin());
                m_avcCBox.ppsLength = naluUnit.size;
            }
            m_spsPpsReady = (m_avcCBox.spsLength > 0 && m_avcCBox.ppsLength > 0);
        }
        else if (naluUnit.type == SliceIdr)
        {
            if (!m_spsPpsReady)
            {
                std::cout << "NO SPS PPS RETURN:" << std::endl;
                continue;
            }

            std::cout << "NALU: IDR:" << std::endl;

            // 构建视频帧结构体
            VideoFrame frame;
            frame.codecId = AV_CODEC_ID_H264;
            frame.size = naluUnit.size + 4;       // FFmpeg要求NALU前面加4字节长度信息
            frame.data[0] = naluUnit.size >> 24;  // 第一个字节存储NALU长度的高8位
            frame.data[1] = naluUnit.size >> 16;  // 第二个字节存储NALU长度的次高8位
            frame.data[2] = naluUnit.size >> 8;   // 第三个字节存储NALU长度的次低8位
            frame.data[3] = naluUnit.size & 0xFF; // 第四个字节存储NALU长度的低8位
            // 剩下的位置拷贝NALU数据
            std::copy(naluUnit.data.begin(), naluUnit.data.end(), frame.data.begin() + 4);

            // 最后把封装好的数据写入
            writeVideoFrame(frame, 1);
        }

        // 普通帧的处理和I帧类似
        else if (naluUnit.type == Slice)
        {
            if (!m_spsPpsReady)
            {
                std::cout << "NO SPS PPS RETURN:" << std::endl;
                continue;
            }
            VideoFrame frame;
            frame.codecId = AV_CODEC_ID_H264;
            frame.size = naluUnit.size + 4;
            frame.data[0] = naluUnit.size >> 24;
            frame.data[1] = naluUnit.size >> 16;
            frame.data[2] = naluUnit.size >> 8;
            frame.data[3] = naluUnit.size & 0xFF;
            std::copy(naluUnit.data.begin(), naluUnit.data.end(), frame.data.begin() + 4);
            writeVideoFrame(frame, 0);
        }
    }
}

// 从完整的H.264数据缓冲区中读取一个NALU单元
bool VideoWriter::readOneNaluFromBuff(const std::vector<uint8_t> &buffer, size_t bufferSize, size_t &offset, H264Nalu &nalu)
{
    size_t i = offset;
    while (i < bufferSize)
    {
        // 首先判断是否有两个连续的0x00字节
        // 因为NALU单元的起始码是0x000001或0x00000001，所以需要先找到两个0x00字节
        // 这里i++会在下一个表达式判断前完成自增操作
        if (buffer[i++] == 0x00 && buffer[i++] == 0x00)
        {
            uint8_t c = buffer[i++];
            // 判断第三个字节是否为0x01，或者第三个字节为0x00且第四个字节为0x01来确定为NALU起始码
            // 并将游标移动到0x01字节的下一个位置
            if ((c == 0x01) || ((c == 0x00) && (buffer[i++] == 0x01)))
            {
                // 记录当前NALU单元的起始位置
                size_t pos = i;
                // 初始化下一个NALU单元的起始码长度，默认为4字节起始码
                size_t num = 4;

                // 继续向后查找下一个NALU单元的起始码位置
                // 并设定下一个NALU单元的起始码长度
                while (pos < bufferSize)
                {
                    if (buffer[pos++] == 0x00 && buffer[pos++] == 0x00)
                    {
                        c = buffer[pos++];
                        if (c == 0x01)
                        {
                            num = 3;
                            break;
                        }
                        else if ((c == 0x00) && (buffer[pos++] == 0x01))
                        {
                            num = 4;
                            break;
                        }
                    }
                }

                /*
                i是当前NALU单元的数据起始位置

                情况一：pos等于bufferSize（表示没有下一个NALU单元了）
                此时，当前NALU单元的长度为bufferSize - i

                情况二：pos小于bufferSize（表示找到了下一个NALU单元的起始码）
                此时pos的位置是下一个NALU单元数据的起始位置
                于是需要先减去num（下一个NALU单元起始码的长度），此时的位置就是当前NALU单元的结束位置
                当前NALU单元的长度就是(结束位置 - i)
                */
                if (pos == bufferSize)
                {
                    nalu.size = pos - i;
                }
                else
                {
                    nalu.size = pos - num - i;
                }

                // NALU起始码后面第一个字节的低5位表示NALU单元的类型
                nalu.type = (H264NalType)(buffer[i] & 0x1F);
                nalu.data.clear();
                nalu.data.assign(buffer.begin() + i, buffer.begin() + i + nalu.size);

                // 更新offset位置到当前NALU单元的结束位置
                offset = pos - num;
                return true;
            }
        }
    }
    return false;
}

// keyFlags: 1表示关键帧，0表示非关键帧
void VideoWriter::writeVideoFrame(const VideoFrame &pData, int keyFlags)
{
    if (!m_recordStatus)
    {
        return;
    }

    if (m_pVideoStream == nullptr)
    {
        initVideoWriter();
    }

    m_startRecordStatus = true;

    AVPacket packet = {};

    packet.stream_index = m_pVideoStream->index;

    // 新逻辑：固定帧率，直接按 stream time_base 递增
    const int64_t frameIndex = static_cast<int64_t>(m_videoFrameCount);
    packet.duration = 1;

    // 使用帧序号作为PTS
    // 没有B帧，DTS等于PTS
    packet.pts = packet.dts = frameIndex;

    // 更新累计帧数
    m_videoFrameCount += 1;

    packet.size = static_cast<int>(pData.size);
    packet.data = const_cast<uint8_t *>(pData.data.data());
    packet.flags |= (keyFlags > 0) ? AV_PKT_FLAG_KEY : 0;

    // 写入一帧视频数据构建的packet
    int ret = av_interleaved_write_frame(m_pFormatContex, &packet);

    if (ret < 0)
    {
        std::cerr << "Call av_write_frame function failed, codecid:" << pData.codecId
                  << ", size:" << packet.size << ", dts:" << packet.dts
                  << ", duration:" << packet.duration << ", return:" << ret << std::endl;
    }

    av_packet_unref(&packet);
}

void VideoWriter::writeAudioData(std::vector<uint8_t> buffer, size_t length)
{
    if (!m_recordStatus)
    {
        return;
    }

    if (!m_startRecordStatus)
    {
        return;
    }

    if (buffer.empty() || length == 0)
    {
        return;
    }

    AudioFrame frame;
    frame.codecId = AV_CODEC_ID_AAC;
    frame.size = std::min(length, frame.data.size());
    std::copy_n(buffer.begin(), frame.size, frame.data.begin());
    writeAudioStream(frame);
}

void VideoWriter::writeAudioStream(const AudioFrame &frame)
{
    if (!m_recordStatus)
    {
        return;
    }
    if (m_pAudioStream == nullptr || frame.size == 0)
    {
        return;
    }

    AVPacket packet = {};

    packet.stream_index = m_pAudioStream->index;
    packet.duration = 1;

    packet.size = static_cast<int>(frame.size);
    packet.data = const_cast<uint8_t *>(frame.data.data());
    packet.dts = packet.pts = m_audioFrameCount;
    m_audioFrameCount += 1;

    int ret = av_interleaved_write_frame(m_pFormatContex, &packet);

    if (ret < 0)
    {
        std::cerr << "Call av_write_frame function failed, codecid:" << frame.codecId
                  << ", size:" << packet.size << ", dts:" << packet.dts
                  << ", duration:" << packet.duration << ", return:" << ret << std::endl;
        av_packet_unref(&packet);
        return;
    }

    av_packet_unref(&packet);
}

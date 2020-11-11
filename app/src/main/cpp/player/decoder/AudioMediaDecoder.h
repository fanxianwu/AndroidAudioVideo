//
// Created by 字节流动 on 2020/10/10.
//

#ifndef LEARNFFMPEG_AUDIOMEDIADECODER_H
#define LEARNFFMPEG_AUDIOMEDIADECODER_H

extern "C" {
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
};

#include <render/audio/AudioRender.h>
#include "MediaDecoder.h"
// 音频编码采样率
static const int AUDIO_DST_SAMPLE_RATE = 44100;
// 音频编码通道数
static const int AUDIO_DST_CHANNEL_COUNTS = 2;
// 音频编码声道格式
static const uint64_t AUDIO_DST_CHANNEL_LAYOUT = AV_CH_LAYOUT_STEREO;
// 音频编码比特率
static const int AUDIO_DST_BIT_RATE = 64000;
// ACC音频一帧采样数
static const int ACC_NB_SAMPLES = 1024;


class AudioMediaDecoder : public MediaDecoder {
public:
    AudioMediaDecoder(AVCodecContext *avCodecContext, AVStream *avStream, int streamIndex, PlayerState *playerState);

    virtual ~AudioMediaDecoder();

    virtual void Start();

    virtual void Stop();

    virtual void Flush();

    virtual void Run();

    int GetAudioFrame(AVFrame *frame);

    void Wait(int timeMs);

private:
    void InitAudioRender();
    void UnInitAudioRender();
    int DecodeAudio();
    thread *m_Thread = nullptr;

    AVPacket *m_Packet = nullptr;
    int64_t m_NextPts;
    bool m_IsPacketPending = false;

    const AVSampleFormat DST_SAMPLT_FORMAT = AV_SAMPLE_FMT_S16;

    AudioRender  *m_AudioRender = nullptr;

    //audio resample context
    SwrContext   *m_SwrContext = nullptr;
    uint8_t      *m_AudioOutBuffer = nullptr;
    //number of sample per channel
    int           m_nbSamples = 0;
    //dst frame data size
    int           m_DstFrameDataSze = 0;

    volatile int m_WaitTime = 0;
};


#endif //LEARNFFMPEG_AUDIOMEDIADECODER_H

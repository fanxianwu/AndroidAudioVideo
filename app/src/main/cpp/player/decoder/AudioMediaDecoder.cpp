//
// Created by 字节流动 on 2020/10/10.
//

#include <unistd.h>
#include <LogUtil.h>
#include <render/audio/OpenSLRender.h>
#include "AudioMediaDecoder.h"

AudioMediaDecoder::AudioMediaDecoder(AVCodecContext *avCodecContext, AVStream *avStream,
                                     int streamIndex, PlayerState *playerState) : MediaDecoder(
        avCodecContext, avStream, streamIndex, playerState) {

    m_Packet = av_packet_alloc();
}

AudioMediaDecoder::~AudioMediaDecoder() {
    unique_lock<mutex> lock(m_Mutex);
    if (m_Packet) {
        av_packet_free(&m_Packet);
        m_Packet = nullptr;
    }
}

void AudioMediaDecoder::Start() {
    MediaDecoder::Start();
    if(m_Thread == nullptr) {
        m_Thread = new thread(DoAsyncDecoding, this);
    }
}

void AudioMediaDecoder::Stop() {
    LOGCATE("AudioMediaDecoder::Stop");
    MediaDecoder::Stop();
    if(m_Thread != nullptr) {
        m_Thread->join();
        delete m_Thread;
        m_Thread = nullptr;
    }
}

void AudioMediaDecoder::Flush() {
    MediaDecoder::Flush();

    if(m_AudioRender != nullptr)
        m_AudioRender->ClearAudioCache();
}

void AudioMediaDecoder::Run() {
    DecodeAudio();
}

int AudioMediaDecoder::DecodeAudio() {
    AVFrame *frame = av_frame_alloc();
    int result = 0;

    if (!frame) {
        return AVERROR(ENOMEM);
    }

    InitAudioRender();

    for (;;) {

        while (m_PlayerState->m_PauseRequest && !m_PlayerState->m_AbortRequest)
        {
            usleep(5 * 1000);
        }

        result = GetAudioFrame(frame);
        if(result < 0) {
            break;
        }

        unique_lock<mutex> lock(m_PlayerState->m_Mutex);
        m_PlayerState->m_CurTimestamp = frame->pts * av_q2d(m_AvStream->time_base) * 1000; // ms
        lock.unlock();
        if(m_MsgCallback != nullptr) {
            LOGCATE("AudioMediaDecoder::DecodeAudio CurTimestamp=%f", m_PlayerState->m_CurTimestamp / 1000.0f);
            m_MsgCallback(m_MsgContext, PLAYER_MSG_UPDATE_TIME, m_PlayerState->m_CurTimestamp / 1000.0f);
        }

        if(m_WaitTime > 0) {
            av_usleep(10 * 1000);
            m_WaitTime = 0;
        }

        if(m_AudioRender) {
            result = swr_convert(m_SwrContext, &m_AudioOutBuffer, m_DstFrameDataSze / 2, (const uint8_t **) frame->data, frame->nb_samples);
            if (result > 0 ) {
                m_AudioRender->RenderAudioFrame(m_AudioOutBuffer, m_DstFrameDataSze);
            }
        }
    }

    UnInitAudioRender();

    av_frame_free(&frame);
    frame = nullptr;
    return result;
}

int AudioMediaDecoder::GetAudioFrame(AVFrame *frame) {
    LOGCATE("AudioMediaDecoder::GetAudioFrame line=%d", __LINE__);
    int got_frame = 0;
    int ret = 0;

    if (!frame) {
        return AVERROR(ENOMEM);
    }
    av_frame_unref(frame);

    do {

        if (m_AbortRequest || m_PlayerState->m_AbortRequest) {
            ret = -1;
            break;
        }

        if (m_PlayerState->m_SeekRequest) {
            continue;
        }

        AVPacket pkt;
        if (m_IsPacketPending) {
            av_packet_move_ref(&pkt, m_Packet);
            m_IsPacketPending = false;
        } else {
            if (m_PacketQueue->GetPacket(&pkt) < 0) {
                LOGCATE("AudioMediaDecoder::GetAudioFrame line=%d", __LINE__);
                ret = -1;
                break;
            }
            LOGCATE("AudioMediaDecoder::GetAudioFrame line=%d", __LINE__);
        }

        // 将数据包解码
        unique_lock<mutex> playerStateLock(m_PlayerState->m_Mutex);
        ret = avcodec_send_packet(m_AvCodecContext, &pkt);
        if (ret < 0) {
            // 一次解码无法消耗完AVPacket中的所有数据，需要重新解码
            if (ret == AVERROR(EAGAIN)) {
                av_packet_move_ref(m_Packet, &pkt);
                m_IsPacketPending = true;
            } else {
                av_packet_unref(&pkt);
                m_IsPacketPending = false;
            }
            playerStateLock.unlock();
            continue;
        }

        // 获取解码得到的音频帧AVFrame
        ret = avcodec_receive_frame(m_AvCodecContext, frame);
        playerStateLock.unlock();
        // 释放数据包的引用，防止内存泄漏
        av_packet_unref(m_Packet);
        if (ret < 0) {
            av_frame_unref(frame);
            got_frame = 0;
            continue;
        } else {
            got_frame = 1;
            if (frame->pts == AV_NOPTS_VALUE && m_NextPts != AV_NOPTS_VALUE) {
                frame->pts = m_NextPts;
            }

            if (frame->pts != AV_NOPTS_VALUE) {
                m_NextPts = frame->pts + frame->nb_samples;
            }
        }
    } while (!got_frame);

    if (ret < 0) {
        return -1;
    }

    return got_frame;
}

void AudioMediaDecoder::InitAudioRender() {
    LOGCATE("AudioMediaDecoder::InitAudioRender");
    AVCodecContext *codeCtx = GetCodecContext();
    m_AudioRender = new OpenSLRender();
    m_SwrContext = swr_alloc();

    av_opt_set_int(m_SwrContext, "in_channel_layout", codeCtx->channel_layout, 0);
    av_opt_set_int(m_SwrContext, "out_channel_layout", AUDIO_DST_CHANNEL_LAYOUT, 0);

    av_opt_set_int(m_SwrContext, "in_sample_rate", codeCtx->sample_rate, 0);
    av_opt_set_int(m_SwrContext, "out_sample_rate", AUDIO_DST_SAMPLE_RATE, 0);

    av_opt_set_sample_fmt(m_SwrContext, "in_sample_fmt", codeCtx->sample_fmt, 0);
    av_opt_set_sample_fmt(m_SwrContext, "out_sample_fmt", DST_SAMPLT_FORMAT,  0);

    swr_init(m_SwrContext);

    LOGCATE("AudioMediaDecoder::InitAudioRender audio metadata sample rate: %d, channel: %d, format: %d, frame_size: %d, layout: %lld",
            codeCtx->sample_rate, codeCtx->channels, codeCtx->sample_fmt, codeCtx->frame_size,codeCtx->channel_layout);

    // resample params
    m_nbSamples = (int)av_rescale_rnd(ACC_NB_SAMPLES, AUDIO_DST_SAMPLE_RATE, codeCtx->sample_rate, AV_ROUND_UP);
    m_DstFrameDataSze = av_samples_get_buffer_size(NULL, AUDIO_DST_CHANNEL_COUNTS,m_nbSamples, DST_SAMPLT_FORMAT, 1);

    LOGCATE("AudioMediaDecoder::InitAudioRender [m_nbSamples, m_DstFrameDataSze]=[%d, %d]", m_nbSamples, m_DstFrameDataSze);

    m_AudioOutBuffer = (uint8_t *) malloc(m_DstFrameDataSze);

    m_AudioRender->Init();

}

void AudioMediaDecoder::UnInitAudioRender() {
    LOGCATE("AudioMediaDecoder::UnInitAudioRender");
    if(m_AudioRender)
        m_AudioRender->UnInit();

    if(m_AudioOutBuffer) {
        free(m_AudioOutBuffer);
        m_AudioOutBuffer = nullptr;
    }

    if(m_SwrContext) {
        swr_free(&m_SwrContext);
        m_SwrContext = nullptr;
    }
}

void AudioMediaDecoder::Wait(int timeMs) {
    m_WaitTime = timeMs;
}



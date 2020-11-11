//
// Created by 字节流动 on 2020/10/10.
//

#include <LogUtil.h>
#include "VideoMediaDecoder.h"

VideoMediaDecoder::VideoMediaDecoder(AVFormatContext *avFormatContext,
                                     AVCodecContext *avCodecContext, AVStream *avStream,
                                     int streamIndex, PlayerState *playerState)
                                     :MediaDecoder(avCodecContext, avStream, streamIndex, playerState) {
    m_FormatContext = avFormatContext;
    m_FrameQueue = new AVFrameQueue(VIDEO_QUEUE_SIZE, 1);

    AVDictionaryEntry *entry = av_dict_get(avStream->metadata, "rotate", NULL, AV_DICT_MATCH_CASE);
    if (entry && entry->value) {
        m_FrameRotateAngle = atoi(entry->value);
    } else {
        m_FrameRotateAngle = 0;
    }
}

VideoMediaDecoder::~VideoMediaDecoder() {
    m_FormatContext = nullptr;
    if(m_FrameQueue != nullptr) {
        m_FrameQueue->Abort();
        m_FrameQueue->Flush();
        delete m_FrameQueue;
        m_FrameQueue = nullptr;
    }
}

void VideoMediaDecoder::Start() {
    MediaDecoder::Start();
    if(m_FrameQueue != nullptr) {
        m_FrameQueue->Start();
    }

    if(m_Thread == nullptr) {
        m_Thread = new thread(DoAsyncDecoding, this);
    }
}

void VideoMediaDecoder::Stop() {
    MediaDecoder::Stop();
    if(m_FrameQueue != nullptr) {
        m_FrameQueue->Abort();
    }
    if(m_Thread != nullptr) {
        m_Thread->join();
        delete m_Thread;
        m_Thread = nullptr;
    }
}

void VideoMediaDecoder::Flush() {
    MediaDecoder::Flush();
    if(m_FrameQueue != nullptr) {
        m_FrameQueue->Flush();
    }
}

void VideoMediaDecoder::Run() {
    DecodeVideo();
}

int VideoMediaDecoder::GetRotateAngle() {
    return m_FrameRotateAngle;
}

AVFrameQueue *VideoMediaDecoder::GetFrameQueue() {
    return m_FrameQueue;
}

int VideoMediaDecoder::GetFrameQueueSize() {
    return m_FrameQueue ? m_FrameQueue->GetFrameSize() : 0;
}

int VideoMediaDecoder::DecodeVideo() {
    AVFrame *frame = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    Frame *vp;
    int got_picture;
    int result = 0;

    AVRational tb = m_AvStream->time_base;
    AVRational frame_rate = av_guess_frame_rate(m_FormatContext, m_AvStream, NULL);

    if (!frame || !packet) {
        result = AVERROR(ENOMEM);
        goto EXIT;
    }

    for (;;) {

        if (m_AbortRequest || m_PlayerState->m_AbortRequest) {
            result = -1;
            break;
        }

        if (m_PlayerState->m_SeekRequest) {
            continue;
        }

        if (m_PacketQueue->GetPacket(packet) < 0) {
            result = -1;
            break;
        }

        // 送去解码
        long long startTime = GetSysCurrentTime();//统计解码一帧的耗时
        unique_lock<mutex> playerStateLock(m_PlayerState->m_Mutex);
        result = avcodec_send_packet(m_AvCodecContext, packet);
        if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
            av_packet_unref(packet);
            playerStateLock.unlock();
            continue;
        }
        LOGCATE("VideoMediaDecoder::DecodeVideo packet->flags=%d, %d", packet->flags, AV_PKT_FLAG_KEY);

        // 得到解码帧
        result = avcodec_receive_frame(m_AvCodecContext, frame);
        playerStateLock.unlock();
        LOGCATE("VideoMediaDecoder::DecodeVideo decode one frame cost time %lld ms", GetSysCurrentTime() - startTime);
        if (result < 0 && result != AVERROR_EOF) {
            av_frame_unref(frame);
            av_packet_unref(packet);
            continue;
        } else {
            got_picture = 1;

            // 默认情况下需要重排pts的
            frame->pts = av_frame_get_best_effort_timestamp(frame);
        }

        if (got_picture) {

            // 取出帧
            if (!(vp = m_FrameQueue->PeekWritable())) {
                result = -1;
                break;
            }

            // 复制参数
            vp->uploaded = 0;
            vp->width = frame->width;
            vp->height = frame->height;
            vp->format = frame->format;
            vp->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb) * 1000; //ms
            vp->duration = frame_rate.num && frame_rate.den
                           ? av_q2d((AVRational){frame_rate.den, frame_rate.num}) : 0;
            av_frame_move_ref(vp->frame, frame);

            // 入队帧
            m_FrameQueue->PushFrame();
        }

        // 释放数据包和缓冲帧的引用，防止内存泄漏
        av_frame_unref(frame);
        av_packet_unref(packet);
    }

EXIT:
    if(frame != nullptr) {
        av_frame_free(&frame);
        frame = nullptr;
    }

    if(packet != nullptr) {
        av_packet_free(&packet);
        packet = nullptr;
    }

    return result;
}

void VideoMediaDecoder::RequestRender() {
    if(m_MsgCallback != nullptr) {
        m_MsgCallback(m_MsgContext, PLAYER_MSG_REQUEST_RENDER, 0);
    }
}

//
// Created by 字节流动 on 2020/10/10.
//

#ifndef LEARNFFMPEG_MEDIADECODER_H
#define LEARNFFMPEG_MEDIADECODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/time.h>
#include <libavcodec/jni.h>
};

#include <thread>
#include <PlayerState.h>
#include <queue/AVPacketQueue.h>

enum PlayerMsg {
    PLAYER_MSG_PLAYER_ERROR,
    PLAYER_MSG_PLAYER_READY,
    PLAYER_MSG_PLAYER_DONE,
    PLAYER_MSG_REQUEST_RENDER,
    PLAYER_MSG_UPDATE_TIME
};

typedef void (*PlayerMessageCallback)(void*, int, float);

class MediaDecoder {
public:
    MediaDecoder(AVCodecContext *avCodecContext, AVStream *avStream, int streamIndex, PlayerState *playerState);

    virtual ~MediaDecoder();

    virtual void Start();

    virtual void Stop();

    virtual void Flush();

    virtual void Run();

    int PushPacket(AVPacket *avPacket);

    int GetPacketSize();

    int GetStreamIndex();

    AVStream *GetStream() {
        return m_AvStream;
    }

    AVCodecContext *GetCodecContext() {
        return m_AvCodecContext;
    }

    void SetMessageCallback(void *context, PlayerMessageCallback callback) {
        m_MsgContext = context;
        m_MsgCallback = callback;
    }

protected:
    static void DoAsyncDecoding(MediaDecoder *decoder);

    mutex m_Mutex;
    condition_variable m_CondVar;
    PlayerState *m_PlayerState = nullptr;
    AVPacketQueue *m_PacketQueue = nullptr;
    AVCodecContext *m_AvCodecContext = nullptr;
    AVStream *m_AvStream = nullptr;
    int m_StreamIndex = -1;
    volatile int m_AbortRequest = 0;
    void * m_MsgContext = nullptr;
    PlayerMessageCallback m_MsgCallback = nullptr;
};


#endif //LEARNFFMPEG_MEDIADECODER_H

//
// Created by 字节流动 on 2020/10/10.
//

#ifndef LEARNFFMPEG_VIDEOMEDIADECODER_H
#define LEARNFFMPEG_VIDEOMEDIADECODER_H

#include <queue/AVFrameQueue.h>
#include "MediaDecoder.h"

#define VIDEO_QUEUE_SIZE 10

class VideoMediaDecoder : public MediaDecoder {
public:
    VideoMediaDecoder(AVFormatContext *avFormatContext, AVCodecContext *avCodecContext, AVStream *avStream, int streamIndex, PlayerState *playerState);

    virtual ~VideoMediaDecoder();

    virtual void Start();

    virtual void Stop();

    virtual void Flush();

    virtual void Run();

    int GetRotateAngle();

    AVFrameQueue *GetFrameQueue();

    int GetFrameQueueSize();

    void RequestRender();

private:
    int DecodeVideo();

    AVFormatContext *m_FormatContext = nullptr;
    AVFrameQueue *m_FrameQueue = nullptr;
    int m_FrameRotateAngle = 0;
    thread *m_Thread = nullptr;
};


#endif //LEARNFFMPEG_VIDEOMEDIADECODER_H

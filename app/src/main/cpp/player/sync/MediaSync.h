//
// Created by 字节流动 on 2020/10/16.
//

#ifndef LEARNFFMPEG_MEDIASYNC_H
#define LEARNFFMPEG_MEDIASYNC_H

extern "C" {
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
}

#include <render/video/VideoRender.h>
#include <decoder/VideoMediaDecoder.h>
#include <decoder/AudioMediaDecoder.h>

using namespace std;

#define AV_SYNC_THRESHOLD 25 //同步阈值设为 25 ms

class MediaSync {
public:
    MediaSync(PlayerState *playerState, VideoMediaDecoder *videoMediaDecoder, AudioMediaDecoder *audioMediaDecoder);
    virtual ~MediaSync();

    void Start();
    void Stop();

    void SetVideoRender(VideoRender *videoRender);

private:
    static void DoAVSync(MediaSync *mediaSync);
    void Run();
    void InitVideoRender();
    void UnInitVideoRender();
    void RenderVideo(AVFrame *frame);

private:
    const AVPixelFormat DST_PIXEL_FORMAT = AV_PIX_FMT_RGBA;

    PlayerState *m_PlayerState = nullptr;

    VideoMediaDecoder *m_VideoDecoder = nullptr;
    AudioMediaDecoder *m_AudioDecoder = nullptr;

    mutex m_Mutex;
    condition_variable m_ConVar;
    thread *m_Thread = nullptr;

    int m_VideoWidth = 0;
    int m_VideoHeight = 0;
    int m_RenderWidth = 0;
    int m_RenderHeight = 0;

    AVFrame *m_RGBAFrame = nullptr;
    uint8_t *m_FrameBuffer = nullptr;
    VideoRender *m_VideoRender = nullptr;
    SwsContext *m_SwsContext = nullptr;
};


#endif //LEARNFFMPEG_MEDIASYNC_H

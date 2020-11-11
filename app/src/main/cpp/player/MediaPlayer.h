//
// Created by 字节流动 on 2020/10/17.
//

#ifndef LEARNFFMPEG_MEDIAPLAYER_H
#define LEARNFFMPEG_MEDIAPLAYER_H

#include <jni.h>
#include <decoder/VideoMediaDecoder.h>
#include <decoder/AudioMediaDecoder.h>
#include <sync/MediaSync.h>
#include "VideoRender.h"

#define JAVA_PLAYER_EVENT_CALLBACK_API_NAME "playerEventCallback"

#define MEDIA_PARAM_VIDEO_WIDTH         0x0001
#define MEDIA_PARAM_VIDEO_HEIGHT        0x0002
#define MEDIA_PARAM_VIDEO_DURATION      0x0003
#define MEDIA_PARAM_ROTATE_ANGLE        0x0004

class MediaPlayer {
public:
    MediaPlayer(){};
    ~MediaPlayer(){};

    void Init(JNIEnv *jniEnv, jobject obj, char *url, int renderType, jobject surface);
    void UnInit();

    void Play();
    void Pause();
    void Stop();
    void SeekToPosition(float position);
    long GetMediaParams(int paramType);

private:
    static void AsyncMediaPlay(MediaPlayer *player);
    int InitPlayerContext();
    int PrepareDecoder(int streamIndex, int mediaType);
    int ReadPackets();
    int UnInitPlayerContext();
    void OnPlayerReady();
    void OnPlayerDone();
    JNIEnv *GetJNIEnv(bool *isAttach);
    jobject GetJavaObj();
    JavaVM *GetJavaVM();

    static void PostMessage(void *context, int msgType, float msgCode);

    JavaVM *m_JavaVM = nullptr;
    jobject m_JavaObj = nullptr;

    VideoMediaDecoder *m_VideoDecoder = nullptr;
    AudioMediaDecoder *m_AudioDecoder = nullptr;

    MediaSync *m_MediaSync = nullptr;

    VideoRender *m_VideoRender = nullptr;

    //锁和条件变量
    mutex               m_Mutex;
    condition_variable  m_Cond;
    thread             *m_Thread = nullptr;

    //播放器状态
    PlayerState *m_PlayerState = nullptr;

    //封装格式上下文
    AVFormatContext *m_AVFormatCtx = nullptr;

    //解码器上下文
    AVCodecContext  *m_AudioCodecCtx = nullptr;
    AVCodecContext  *m_VideoCodecCtx = nullptr;


};


#endif //LEARNFFMPEG_MEDIAPLAYER_H

//
// Created by 字节流动 on 2020/10/17.
//

#include <render/video/NativeRender.h>
#include <render/audio/OpenSLRender.h>
#include <render/video/VideoGLRender.h>
#include <render/video/VRGLRender.h>
#include "MediaPlayer.h"

void MediaPlayer::Init(JNIEnv *jniEnv, jobject obj, char *url, int videoRenderType, jobject surface) {
    jniEnv->GetJavaVM(&m_JavaVM);
    m_JavaObj = jniEnv->NewGlobalRef(obj);
    m_PlayerState = new PlayerState();
    strcpy(m_PlayerState->m_Url, url);
    av_jni_set_java_vm(m_JavaVM, nullptr);
    if(videoRenderType == VIDEO_RENDER_OPENGL) {
        m_VideoRender = VideoGLRender::GetInstance();
    } else if (videoRenderType == VIDEO_RENDER_ANWINDOW) {
        m_VideoRender = NativeRender::GetInstance(jniEnv, surface);
    } else if (videoRenderType == VIDEO_RENDER_3D_VR) {
        m_VideoRender = VRGLRender::GetInstance();
    }

    m_Thread = new thread(AsyncMediaPlay, this);
}

void MediaPlayer::UnInit() {
    LOGCATE("MediaPlayer::UnInit");
    Stop();
    if(m_Thread != nullptr) {
        m_Thread->join();
        delete m_Thread;
        m_Thread = nullptr;
    }

    m_VideoRender = nullptr;

    NativeRender::ReleaseInstance();
    VideoGLRender::ReleaseInstance();
    VRGLRender::ReleaseInstance();

    bool isAttach = false;
    GetJNIEnv(&isAttach)->DeleteGlobalRef(m_JavaObj);
    if(isAttach)
        GetJavaVM()->DetachCurrentThread();

}

void MediaPlayer::Play() {
    LOGCATE("MediaPlayer::Play");
    unique_lock<mutex> lock(m_Mutex);
    m_PlayerState->m_PauseRequest = 0;
    m_PlayerState->m_AbortRequest = 0;
    m_Cond.notify_all();
}

void MediaPlayer::Pause() {
    LOGCATE("MediaPlayer::Pause");
    unique_lock<mutex> lock(m_Mutex);
    m_PlayerState->m_PauseRequest = 1;
    m_Cond.notify_all();
}

void MediaPlayer::Stop() {
    LOGCATE("MediaPlayer::Stop");
    unique_lock<mutex> lock(m_Mutex);
    m_PlayerState->m_AbortRequest = 1;
    m_Cond.notify_all();
}

void MediaPlayer::SeekToPosition(float position) {
    LOGCATE("MediaPlayer::SeekToPosition position=%f", position);
    if(position < 0 || m_PlayerState->m_Duration < 0) return;

    unique_lock<mutex> lock(m_Mutex);
    while(m_PlayerState->m_SeekRequest) {
        m_Cond.wait(lock);
    }
    lock.unlock();

    if(m_PlayerState->m_SeekRequest == 0) {
        int64_t seek_pos = av_rescale(position, AV_TIME_BASE, 1); // s to us
        int64_t start_time = m_AVFormatCtx ? m_AVFormatCtx->start_time : 0;
        if (start_time > 0 && start_time != AV_NOPTS_VALUE) {
            seek_pos += start_time;
        }
        lock.lock();
        m_PlayerState->m_SeekRequest = 1;
        m_PlayerState->m_SeekPosition = seek_pos;
        m_PlayerState->m_PauseRequest = 0;
        m_Cond.notify_all();
        lock.unlock();
    }
}

long MediaPlayer::GetMediaParams(int paramType) {
    LOGCATE("MediaPlayer::GetMediaParams paramType=%d", paramType);
    long value = 0;
    switch(paramType)
    {
        case MEDIA_PARAM_VIDEO_WIDTH:
            value = m_VideoCodecCtx != nullptr ? m_VideoCodecCtx->width : 0;
            break;
        case MEDIA_PARAM_VIDEO_HEIGHT:
            value = m_VideoCodecCtx != nullptr ? m_VideoCodecCtx->height : 0;
            break;
        case MEDIA_PARAM_VIDEO_DURATION:
            if (m_PlayerState != nullptr) {
                value = m_PlayerState->m_Duration;
            } else {
                value = 0;
            }
            break;
        case MEDIA_PARAM_ROTATE_ANGLE:
            value = m_VideoDecoder != nullptr ? m_VideoDecoder->GetRotateAngle() : 0;
            break;
    }
    return value;
}

JNIEnv *MediaPlayer::GetJNIEnv(bool *isAttach) {
    JNIEnv *env;
    int status;
    if (nullptr == m_JavaVM) {
        LOGCATE("MediaPlayer::GetJNIEnv m_JavaVM == nullptr");
        return nullptr;
    }
    *isAttach = false;
    status = m_JavaVM->GetEnv((void **)&env, JNI_VERSION_1_4);
    if (status != JNI_OK) {
        status = m_JavaVM->AttachCurrentThread(&env, nullptr);
        if (status != JNI_OK) {
            LOGCATE("MediaPlayer::GetJNIEnv failed to attach current thread");
            return nullptr;
        }
        *isAttach = true;
    }
    return env;
}

jobject MediaPlayer::GetJavaObj() {
    return m_JavaObj;
}

JavaVM *MediaPlayer::GetJavaVM() {
    return m_JavaVM;
}

void MediaPlayer::PostMessage(void *context, int msgType, float msgCode) {
    if(context != nullptr)
    {
        MediaPlayer *player = static_cast<MediaPlayer *>(context);
        bool isAttach = false;
        JNIEnv *env = player->GetJNIEnv(&isAttach);
        LOGCATE("MediaPlayer::PostMessage env=%p", env);
        if(env == nullptr)
            return;
        jobject javaObj = player->GetJavaObj();
        jmethodID mid = env->GetMethodID(env->GetObjectClass(javaObj), JAVA_PLAYER_EVENT_CALLBACK_API_NAME, "(IF)V");
        env->CallVoidMethod(javaObj, mid, msgType, msgCode);
        if(isAttach)
            player->GetJavaVM()->DetachCurrentThread();

    }
}

void MediaPlayer::AsyncMediaPlay(MediaPlayer *player) {
    LOGCATE("MediaPlayer::AsyncMediaPlay line=%d", __LINE__);
    int result = -1;
    do {
        result = player->InitPlayerContext();
        if(result != 0) {
            LOGCATE("MediaPlayer::AsyncMediaPlay InitMediaPlayer fail. result=%d", result);
            break;
        }
        player->OnPlayerReady();

        result = player->ReadPackets();
        LOGCATE("MediaPlayer::AsyncMediaPlay line=%d", __LINE__);
        if(result != 0) {
            LOGCATE("MediaPlayer::AsyncMediaPlay ReadPackets fail. result=%d", result);
            break;
        }

    } while (false);

    player->UnInitPlayerContext();

    player->OnPlayerDone();

}

int MediaPlayer::InitPlayerContext() {
    LOGCATE("MediaPlayer::InitMediaPlayer");
    int result = -1;
    do {
        //1.创建封装格式上下文
        m_AVFormatCtx = avformat_alloc_context();

        //2.打开文件
        if(avformat_open_input(&m_AVFormatCtx, m_PlayerState->m_Url, NULL, NULL) != 0)
        {
            LOGCATE("MediaPlayer::InitMediaPlayer avformat_open_input fail.");
            break;
        }

        //3.获取音视频流信息
        if(avformat_find_stream_info(m_AVFormatCtx, NULL) < 0) {
            LOGCATE("MediaPlayer::InitMediaPlayer avformat_find_stream_info fail.");
            break;
        }

        m_PlayerState->m_Duration = m_AVFormatCtx->duration * 1.0 / AV_TIME_BASE;
        m_PlayerState->m_StartTime = m_AVFormatCtx->start_time * 1.0 / AV_TIME_BASE;

        //4.获取音视频流索引
        int audioIndex = -1;
        int videoIndex = -1;
        for(int i=0; i < m_AVFormatCtx->nb_streams; i++) {
            if (m_AVFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                if (audioIndex == -1) {
                    audioIndex = i;
                }
            } else if (m_AVFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                if (videoIndex == -1) {
                    videoIndex = i;
                }
            }
        }

        if(audioIndex == -1 && videoIndex == -1) {
            LOGCATE("MediaPlayer::InitMediaPlayer find stream index fail.");
            result = -1;
            break;
        }

        // 根据媒体流索引准备解码器
        if (audioIndex >= 0) {
            PrepareDecoder(audioIndex, AVMEDIA_TYPE_AUDIO);
        }
        if (videoIndex >= 0) {
            PrepareDecoder(videoIndex, AVMEDIA_TYPE_VIDEO);
        }

        if(m_VideoDecoder == nullptr && m_AudioDecoder == nullptr) {
            LOGCATE("MediaPlayer::InitMediaPlayer fail to create video and audio decoder.");
            result = -1;
            break;
        }

        result = 0;

        m_MediaSync = new MediaSync(m_PlayerState, m_VideoDecoder, m_AudioDecoder);
        m_MediaSync->SetVideoRender(m_VideoRender);

        //启动解码器和同步器
        if(m_VideoDecoder != nullptr)
            m_VideoDecoder->Start();

        if(m_AudioDecoder != nullptr)
            m_AudioDecoder->Start();

        m_MediaSync->Start();

    } while (false);

    return result;
}

int MediaPlayer::PrepareDecoder(int streamIndex, int mediaType) {
    int result = -1;

    AVCodecContext *pCodecContext = nullptr;
    AVCodec *pCodec = nullptr;

    do {
        // 获取解码器参数
        AVCodecParameters *codecParameters = m_AVFormatCtx->streams[streamIndex]->codecpar;

        // 获取解码器
//        switch (codecParameters->codec_id){
//            case AV_CODEC_ID_H264:
//                pCodec = avcodec_find_decoder_by_name("h264_mediacodec"); //硬解码264
//                if(pCodec == nullptr) {
//                    LOGCATE("MediaPlayer::PrepareDecoder avcodec_find_decoder_by_name(\"h264_mediacodec\") fail.");
//                }
//                break;
//            case AV_CODEC_ID_MPEG4:
//                pCodec = avcodec_find_decoder_by_name("mpeg4_mediacodec"); //硬解码mpeg4
//                if(pCodec == nullptr) {
//                    LOGCATE("MediaPlayer::PrepareDecoder avcodec_find_decoder_by_name(\"mpeg4_mediacodec\") fail.");
//                }
//                break;
//            case AV_CODEC_ID_HEVC:
//                pCodec = avcodec_find_decoder_by_name("hevc_mediacodec"); //硬解码265
//                if(pCodec == nullptr) {
//                    LOGCATE("MediaPlayer::PrepareDecoder avcodec_find_decoder_by_name(\"hevc_mediacodec\") fail.");
//                }
//                break;
//            default:
//                break;
//        }

        pCodec = avcodec_find_decoder(codecParameters->codec_id);
        if (pCodec == nullptr) {
            LOGCATE("MediaPlayer::PrepareDecoder avcodec_find_decoder fail.");
            break;
        }

        // 创建解码器上下文
        pCodecContext = avcodec_alloc_context3(pCodec);
        result = avcodec_parameters_to_context(pCodecContext, codecParameters);
        if(result < 0) {
            LOGCATE("MediaPlayer::PrepareDecoder avcodec_parameters_to_context fail.");
            break;
        }

        // 打开解码器
        result = avcodec_open2(pCodecContext, pCodec, NULL);
        if(result < 0) {
            LOGCATE("MediaPlayer::PrepareDecoder avcodec_open2 fail. result=%d", result);
            break;
        }

        switch (mediaType) {
            case AVMEDIA_TYPE_AUDIO: {
                m_AudioCodecCtx = pCodecContext;
                m_AudioDecoder = new AudioMediaDecoder(pCodecContext, m_AVFormatCtx->streams[streamIndex],
                                                       streamIndex, m_PlayerState);
                m_AudioDecoder->SetMessageCallback(this, PostMessage);
                break;
            }

            case AVMEDIA_TYPE_VIDEO: {
                m_VideoCodecCtx = pCodecContext;
                m_VideoDecoder = new VideoMediaDecoder(m_AVFormatCtx, pCodecContext, m_AVFormatCtx->streams[streamIndex],
                                                       streamIndex, m_PlayerState);
                m_VideoDecoder->SetMessageCallback(this, PostMessage);
                break;
            }
            default:
                break;
        }
        result = 0;

    } while (false);

    return result;
}

int MediaPlayer::ReadPackets() {
    if (m_PlayerState->m_PauseRequest) {
        while ((!m_PlayerState->m_AbortRequest) && m_PlayerState->m_PauseRequest) {
            av_usleep(10 * 1000);
        }
    }

    int result = -1;
    AVPacket avPacket, *pPacket = &avPacket;
    for (;;) {
        if (m_PlayerState->m_AbortRequest) {
            break;
        }

        if(m_PlayerState->m_SysTimeBase == 0) {
            unique_lock<mutex> lock(m_PlayerState->m_Mutex);
            m_PlayerState->m_SysTimeBase = GetSysCurrentTime();
        }

        while (m_PlayerState->m_PauseRequest && (!m_PlayerState->m_AbortRequest))
        {
            unique_lock<mutex> lock(m_Mutex);
            m_Cond.wait_for(lock, std::chrono::milliseconds(10));
            {
                unique_lock<mutex> playerStateLock(m_PlayerState->m_Mutex);
                m_PlayerState->m_SysTimeBase = GetSysCurrentTime() - m_PlayerState->m_CurTimestamp;
            }
        }

        // 定位处理
        if (m_PlayerState->m_SeekRequest) {
            int64_t seek_target = m_PlayerState->m_SeekPosition;
            int64_t seek_min = INT64_MIN;
            int64_t seek_max = INT64_MAX;
            LOGCATE("MediaPlayer::ReadPackets avformat_seek_file seek_target=%ld",seek_target);
            int seek_ret = avformat_seek_file(m_AVFormatCtx, -1, seek_min, seek_target, seek_max, 0);
            if (seek_ret < 0) {
                LOGCATE("MediaPlayer::ReadPackets avformat_seek_file fail");
            } else {
                //清空缓存
                if (m_VideoDecoder) {
                    m_VideoDecoder->Flush();
                }

                if (m_AudioDecoder) {
                    m_AudioDecoder->Flush();
                }
                // TODO 更新外部时钟值
            }
            m_PlayerState->m_SeekRequest = 0;
        }
        // 读出数据包
        result = av_read_frame(m_AVFormatCtx, pPacket);
        if (result < 0) {
            // 读取出错，则直接退出
            if (m_AVFormatCtx->pb && m_AVFormatCtx->pb->error) {
                LOGCATE("MediaPlayer::ReadPackets av_read_frame fail");
                break;
            }
            // 判断是否是结尾
            if ((result == AVERROR_EOF || avio_feof(m_AVFormatCtx->pb))) {
                //如果到达结尾，判断队列中是否还有数据，如果没有数据则播放结束，判断是否需要循环播放
                if (!m_PlayerState->m_PauseRequest && (!m_AudioDecoder || m_AudioDecoder->GetPacketSize() == 0)
                    && (!m_VideoDecoder || (m_VideoDecoder->GetPacketSize() == 0
                                          && m_VideoDecoder->GetFrameQueueSize() == 0))) {
                    if (m_PlayerState->m_Loop) {
                        if (m_PlayerState->m_StartTime != AV_NOPTS_VALUE) {
                            SeekToPosition(m_PlayerState->m_StartTime);
                        } else {
                            SeekToPosition(0);
                        }
                    } else if (m_PlayerState->m_AutoExit) {
                        result = AVERROR_EOF;
                        break;
                    }
                }
            }
            // 读取失败时，睡眠 5 ms 继续
            av_usleep(5 * 1000);
            continue;
        }
        if (m_AudioDecoder && pPacket->stream_index == m_AudioDecoder->GetStreamIndex()) {
            m_AudioDecoder->PushPacket(pPacket);
        } else if (m_VideoDecoder && pPacket->stream_index == m_VideoDecoder->GetStreamIndex()) {
            m_VideoDecoder->PushPacket(pPacket);
        } else {
            av_packet_unref(pPacket);
        }
    }
    return result;
}

void MediaPlayer::OnPlayerReady() {
    PostMessage(this, PLAYER_MSG_PLAYER_READY, 0);
}

int MediaPlayer::UnInitPlayerContext() {
    LOGCATE("MediaPlayer::UnInitPlayerContext");
    if(m_MediaSync) {
        m_MediaSync->Stop();
        delete m_MediaSync;
        m_MediaSync = nullptr;
    }

    if(m_VideoDecoder) {
        m_VideoDecoder->Stop();
        delete m_VideoDecoder;
        m_VideoDecoder = nullptr;
    }

    if(m_AudioDecoder) {
        m_AudioDecoder->Stop();
        delete m_AudioDecoder;
        m_AudioDecoder = nullptr;
    }

    if(m_AudioCodecCtx != nullptr) {
        avcodec_close(m_AudioCodecCtx);
        avcodec_free_context(&m_AudioCodecCtx);
        m_AudioCodecCtx = nullptr;
    }

    if(m_VideoCodecCtx != nullptr) {
        avcodec_close(m_VideoCodecCtx);
        avcodec_free_context(&m_VideoCodecCtx);
        m_VideoCodecCtx = nullptr;
    }

    if(m_AVFormatCtx != nullptr) {
        avformat_close_input(&m_AVFormatCtx);
        avformat_free_context(m_AVFormatCtx);
        m_AVFormatCtx = nullptr;
    }
    return 0;
}

void MediaPlayer::OnPlayerDone() {
    PostMessage(this, PLAYER_MSG_PLAYER_DONE, 0);
}









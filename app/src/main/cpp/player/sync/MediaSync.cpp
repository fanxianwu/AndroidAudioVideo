//
// Created by 字节流动 on 2020/10/16.
//

#include "MediaSync.h"

MediaSync::MediaSync(PlayerState *playerState, VideoMediaDecoder *videoMediaDecoder,
                     AudioMediaDecoder *audioMediaDecoder) {
    m_PlayerState  = playerState;
    m_VideoDecoder = videoMediaDecoder;
    m_AudioDecoder = audioMediaDecoder;
}

MediaSync::~MediaSync() {
    m_PlayerState  = nullptr;
    m_VideoDecoder = nullptr;
    m_AudioDecoder = nullptr;
}

void MediaSync::Start() {
    if (m_Thread == nullptr) {
        m_Thread = new thread(DoAVSync, this);
    }
}

void MediaSync::Stop() {
    if(m_Thread != nullptr) {
        m_Thread->join();
        delete m_Thread;
        m_Thread = nullptr;
    }
}

void MediaSync::SetVideoRender(VideoRender *videoRender) {
    m_VideoRender = videoRender;
}

void MediaSync::DoAVSync(MediaSync *mediaSync) {
    if(mediaSync != nullptr)
        mediaSync->Run();
}

void MediaSync::Run() {
    InitVideoRender();

    for (;;) {

        if (m_PlayerState->m_AbortRequest) {
            break;
        }

        AVFrameQueue *frameQueue = m_VideoDecoder->GetFrameQueue();
        unique_lock<mutex> lock(frameQueue->GetQueueMutex());
        if (!m_PlayerState->m_SeekRequest && m_VideoDecoder->GetFrameQueueSize() > 0) {
            int64_t baseTimestamp = m_PlayerState->m_CurTimestamp;
            Frame *curFrame = m_VideoDecoder->GetFrameQueue()->FrontFrame();
            int64_t curTimestamp = curFrame->pts;
            int delayTime =  baseTimestamp - curTimestamp;
            if(delayTime > AV_SYNC_THRESHOLD) {
                m_VideoDecoder->GetFrameQueue()->PopFrame();
                lock.unlock();
                if(delayTime > 200)
                    m_VideoDecoder->GetFrameQueue()->Flush();
                continue;
            }

            while (!(frameQueue->FlushRequest()) && curTimestamp > baseTimestamp && (!m_PlayerState->m_AbortRequest)) {
                int sleepTime = curTimestamp - baseTimestamp;
                sleepTime = sleepTime > AV_SYNC_THRESHOLD ? AV_SYNC_THRESHOLD : sleepTime;
                av_usleep(sleepTime * 1000);
                if(baseTimestamp < m_PlayerState->m_CurTimestamp)
                {
                    baseTimestamp = m_PlayerState->m_CurTimestamp;
                }
                else {
                    break;//
                }
            }

            if(!(frameQueue->FlushRequest())) {
                RenderVideo(curFrame->frame);
                m_VideoDecoder->GetFrameQueue()->PopFrame();
            }
            lock.unlock();

            while (m_PlayerState->m_PauseRequest && (!m_PlayerState->m_AbortRequest)) {
                usleep(5 * 1000);
            }
        } else {
            lock.unlock();
            usleep(5 * 1000);//休眠 5 ms 继续检查队列是否有数据
        }
    }

    UnInitVideoRender();
}

void MediaSync::InitVideoRender() {
    LOGCATE("MediaSync::InitVideoRender");
    if(m_VideoDecoder != nullptr && m_VideoRender != nullptr) {
        m_VideoWidth = m_VideoDecoder->GetCodecContext()->width;
        m_VideoHeight = m_VideoDecoder->GetCodecContext()->height;

        if(m_VideoRender != nullptr) {
            int dstSize[2] = {0};
            m_VideoRender->Init(m_VideoWidth, m_VideoHeight, dstSize);
            m_RenderWidth = dstSize[0];
            m_RenderHeight = dstSize[1];
            m_RGBAFrame = av_frame_alloc();
            int bufferSize = av_image_get_buffer_size(DST_PIXEL_FORMAT, m_RenderWidth, m_RenderHeight, 1);
            m_FrameBuffer = (uint8_t *) av_malloc(bufferSize * sizeof(uint8_t));
            av_image_fill_arrays(m_RGBAFrame->data, m_RGBAFrame->linesize,
                                 m_FrameBuffer, DST_PIXEL_FORMAT, m_RenderWidth, m_RenderHeight, 1);

            m_SwsContext = sws_getContext(m_VideoWidth, m_VideoHeight, m_VideoDecoder->GetCodecContext()->pix_fmt,
                                          m_RenderWidth, m_RenderHeight, DST_PIXEL_FORMAT,
                                          SWS_FAST_BILINEAR, NULL, NULL, NULL);
        }
    }
}

void MediaSync::UnInitVideoRender() {
    LOGCATE("MediaSync::UnInitVideoRender");
    if(m_VideoRender)
        m_VideoRender->UnInit();

    if(m_RGBAFrame != nullptr) {
        av_frame_free(&m_RGBAFrame);
        m_RGBAFrame = nullptr;
    }

    if(m_FrameBuffer != nullptr) {
        free(m_FrameBuffer);
        m_FrameBuffer = nullptr;
    }

    if(m_SwsContext != nullptr) {
        sws_freeContext(m_SwsContext);
        m_SwsContext = nullptr;
    }
}

void MediaSync::RenderVideo(AVFrame *frame) {
    LOGCATE("VideoDecoder::OnFrameAvailable frame=%p", frame);
    if(m_VideoRender != nullptr && frame != nullptr) {
        AVCodecContext *avCodecContext = m_VideoDecoder->GetCodecContext();
        NativeImage image;
        LOGCATE("VideoDecoder::OnFrameAvailable frame[w,h]=[%d, %d],format=%d,[line0,line1,line2]=[%d, %d, %d]", frame->width, frame->height, avCodecContext->pix_fmt, frame->linesize[0], frame->linesize[1],frame->linesize[2]);
        if(m_VideoRender->GetRenderType() == VIDEO_RENDER_ANWINDOW)
        {
            sws_scale(m_SwsContext, frame->data, frame->linesize, 0,
                      m_VideoHeight, m_RGBAFrame->data, m_RGBAFrame->linesize);

            image.format = IMAGE_FORMAT_RGBA;
            image.width = m_RenderWidth;
            image.height = m_RenderHeight;
            image.ppPlane[0] = m_RGBAFrame->data[0];
        } else if(avCodecContext->pix_fmt == AV_PIX_FMT_YUV420P || avCodecContext->pix_fmt == AV_PIX_FMT_YUVJ420P) {
            image.format = IMAGE_FORMAT_I420;
            image.width = frame->width;
            image.height = frame->height;
            image.pLineSize[0] = frame->linesize[0];
            image.pLineSize[1] = frame->linesize[1];
            image.pLineSize[2] = frame->linesize[2];
            image.ppPlane[0] = frame->data[0];
            image.ppPlane[1] = frame->data[1];
            image.ppPlane[2] = frame->data[2];
            if(frame->data[0] && frame->data[1] && !frame->data[2] && frame->linesize[0] == frame->linesize[1] && frame->linesize[2] == 0) {
                // on some android device, output of h264 mediacodec decoder is NV12 兼容某些设备可能出现的格式不匹配问题
                image.format = IMAGE_FORMAT_NV12;
            }
        } else if (avCodecContext->pix_fmt == AV_PIX_FMT_NV12) {
            image.format = IMAGE_FORMAT_NV12;
            image.width = frame->width;
            image.height = frame->height;
            image.pLineSize[0] = frame->linesize[0];
            image.pLineSize[1] = frame->linesize[1];
            image.ppPlane[0] = frame->data[0];
            image.ppPlane[1] = frame->data[1];
        } else if (avCodecContext->pix_fmt == AV_PIX_FMT_NV21) {
            image.format = IMAGE_FORMAT_NV21;
            image.width = frame->width;
            image.height = frame->height;
            image.pLineSize[0] = frame->linesize[0];
            image.pLineSize[1] = frame->linesize[1];
            image.ppPlane[0] = frame->data[0];
            image.ppPlane[1] = frame->data[1];
        } else if (avCodecContext->pix_fmt == AV_PIX_FMT_RGBA) {
            image.format = IMAGE_FORMAT_RGBA;
            image.width = frame->width;
            image.height = frame->height;
            image.pLineSize[0] = frame->linesize[0];
            image.ppPlane[0] = frame->data[0];
        } else {
            sws_scale(m_SwsContext, frame->data, frame->linesize, 0,
                      m_VideoHeight, m_RGBAFrame->data, m_RGBAFrame->linesize);
            image.format = IMAGE_FORMAT_RGBA;
            image.width = m_RenderWidth;
            image.height = m_RenderHeight;
            image.ppPlane[0] = m_RGBAFrame->data[0];
        }
        m_VideoRender->RenderVideoFrame(&image);
        m_VideoDecoder->RequestRender();
    }
}

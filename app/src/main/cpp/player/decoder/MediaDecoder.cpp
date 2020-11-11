//
// Created by 字节流动 on 2020/10/10.
//

#include "MediaDecoder.h"

MediaDecoder::MediaDecoder(AVCodecContext *avCodecContext, AVStream *avStream, int streamIndex,
                           PlayerState *playerState) {
    m_AvCodecContext = avCodecContext;
    m_AvStream = avStream;
    m_StreamIndex = streamIndex;
    m_PlayerState = playerState;
    m_PacketQueue = new AVPacketQueue();
}

MediaDecoder::~MediaDecoder() {
    unique_lock<mutex> lock(m_Mutex);
    if(m_PacketQueue) {
        m_PacketQueue->Abort();
        m_PacketQueue->Flush();
        delete m_PacketQueue;
        m_PacketQueue = nullptr;
    }

    m_AvCodecContext = nullptr;
    m_AvStream = nullptr;
    m_PlayerState = nullptr;
}

void MediaDecoder::Start() {
    if(m_PacketQueue) {
        m_PacketQueue->Start();
    }
    unique_lock<mutex> lock(m_Mutex);
    m_AbortRequest = 0;
    m_CondVar.notify_all();
}

void MediaDecoder::Stop() {
    unique_lock<mutex> lock(m_Mutex);
    m_AbortRequest = 1;
    m_CondVar.notify_all();
    lock.unlock();
    if(m_PacketQueue) {
        m_PacketQueue->Abort();
    }
}

void MediaDecoder::Flush() {
    if(m_PacketQueue) {
        m_PacketQueue->Flush();
    }
    unique_lock<mutex> lock(m_PlayerState->m_Mutex);
    avcodec_flush_buffers(GetCodecContext());
}

void MediaDecoder::Run() {

}

int MediaDecoder::PushPacket(AVPacket *avPacket) {
    if(m_PacketQueue) {
        m_PacketQueue->PushPacket(avPacket);
    }
    return -1;
}

int MediaDecoder::GetPacketSize() {
    return m_PacketQueue ? m_PacketQueue->GetPacketSize() : 0;
}

int MediaDecoder::GetStreamIndex() {
    return m_StreamIndex;
}

void MediaDecoder::DoAsyncDecoding(MediaDecoder *decoder) {
    if(decoder != nullptr)
        decoder->Run();
}

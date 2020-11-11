//
// Created by ByteFlow on 2020/12/21.
//

#include "AVFrameQueue.h"

AVFrameQueue::AVFrameQueue(int max_size, int keep_last) {
    memset(queue, 0, sizeof(Frame) * FRAME_QUEUE_MAX_SIZE);
    this->max_size = FFMIN(max_size, FRAME_QUEUE_MAX_SIZE);
    for (int i = 0; i < this->max_size; ++i) {
        queue[i].frame = av_frame_alloc();
        queue[i].resetPts();
    }
    abort_request = 1;
    rindex = 0;
    windex = 0;
    size = 0;
    m_FlushRequest = false;
 }

AVFrameQueue::~AVFrameQueue() {
    for (int i = 0; i < max_size; ++i) {
        Frame *vp = &queue[i];
        UnRefFrame(vp);
        av_frame_free(&vp->frame);
    }
}

void AVFrameQueue::Start() {
    unique_lock<mutex> lock(m_Mutex);
    abort_request = 0;
    m_CondVar.notify_all();
}

void AVFrameQueue::Abort() {
    unique_lock<mutex> lock(m_Mutex);
    abort_request = 1;
    m_CondVar.notify_all();
}

Frame *AVFrameQueue::FrontFrame() {
    unique_lock<mutex> lock(m_Mutex);
    if(size == 0)
        return nullptr;
    return &queue[rindex];
}

Frame *AVFrameQueue::PeekWritable() {
    unique_lock<mutex> lock(m_Mutex);
    while (size >= max_size && !abort_request) {
        m_CondVar.wait(lock);
    }

    if (abort_request) {
        return nullptr;
    }

    return &queue[windex];
}

void AVFrameQueue::PushFrame() {
    unique_lock<mutex> lock(m_Mutex);
    if (++windex == max_size) {
        windex = 0;
    }
    size++;
    m_CondVar.notify_all();
}

void AVFrameQueue::PopFrame() {
    unique_lock<mutex> lock(m_Mutex);
    if (size == 0) {
        return;
    }
    UnRefFrame(&queue[rindex]);
    if (++rindex == max_size) {
        rindex = 0;
    }
    size--;
    m_CondVar.notify_all();
}

void AVFrameQueue::Flush() {
    m_FlushRequest = true;
    unique_lock<mutex> lock(m_FlushMutex);
    while (GetFrameSize() > 0) {
        PopFrame();
    }
    m_FlushRequest = false;
}

int AVFrameQueue::GetFrameSize() {
    unique_lock<mutex> lock(m_Mutex);
    return size ;
}

void AVFrameQueue::UnRefFrame(Frame *vp) {
    vp->resetPts();
    av_frame_unref(vp->frame);
    avsubtitle_free(&vp->sub);
}

//
// Created by ByteFlow on 2020/12/21.
//

#ifndef LEARNFFMPEG_AVFRAMEQUEUE_H
#define LEARNFFMPEG_AVFRAMEQUEUE_H

#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
};

using namespace std;

#define FRAME_QUEUE_MAX_SIZE 10

typedef struct Frame {
    AVFrame *frame;
    AVSubtitle sub;
    double pts;           /* presentation timestamp for the frame */
    double duration;      /* estimated duration of the frame */
    int width;
    int height;
    int format;
    int uploaded;
    void resetPts() {
        pts = -1;
    }
} Frame;

class AVFrameQueue {

public:
    AVFrameQueue(int max_size, int keep_last);

    virtual ~AVFrameQueue();

    void Start();

    void Abort();

    Frame *FrontFrame();

    Frame *PeekWritable();

    void PushFrame();

    void PopFrame();

    void Flush();

    int GetFrameSize();

    mutex &GetQueueMutex() {
        return m_FlushMutex;
    }

    bool FlushRequest() {
        return m_FlushRequest;
    }
private:
    void UnRefFrame(Frame *vp);

private:
    mutex m_Mutex;
    mutex m_FlushMutex;
    condition_variable m_CondVar;
    int abort_request;
    Frame queue[FRAME_QUEUE_MAX_SIZE];
    int rindex;
    int windex;
    int size;
    int max_size;
    bool m_FlushRequest;
};


#endif //LEARNFFMPEG_AVFRAMEQUEUE_H

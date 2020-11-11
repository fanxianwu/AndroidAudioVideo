//
// Created by 字节流动 on 2020/10/17.
//

#ifndef LEARNFFMPEG_AVPACKETQUEUE_H
#define LEARNFFMPEG_AVPACKETQUEUE_H

#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
};

using namespace std;

typedef struct AVPacketNode {
    AVPacket pkt;
    struct AVPacketNode *next;
} AVPacketNode;

class AVPacketQueue {
public:
    AVPacketQueue();

    virtual ~AVPacketQueue();

    // 入队数据包
    int PushPacket(AVPacket *pkt);

    // 入队空数据包
    int PushNullPacket(int stream_index);

    // 刷新
    void Flush();

    // 终止
    void Abort();

    // 开始
    void Start();

    // 获取数据包
    int GetPacket(AVPacket *pkt);

    // 获取数据包
    int GetPacket(AVPacket *pkt, int block);

    int GetPacketSize();

    int GetSize();

    int64_t GetDuration();

    int IsAbort();

private:
    int Put(AVPacket *pkt);

private:
    mutex m_Mutex;
    condition_variable m_CondVar;
    AVPacketNode *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int64_t duration;
    volatile int abort_request;
};


#endif //LEARNFFMPEG_AVPACKETQUEUE_H

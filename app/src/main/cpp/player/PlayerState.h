//
// Created by 字节流动 on 2020/10/9.
//

#ifndef LEARNFFMPEG_PLAYERSTATE_H
#define LEARNFFMPEG_PLAYERSTATE_H

#include <thread>

#define MAX_PATH 1024
using namespace std;

class PlayerState {
public:
    PlayerState() {
        memset(m_Url, 0, sizeof(MAX_PATH));
    }
    virtual ~PlayerState() {}
public:
    mutex m_Mutex;                  // 互斥锁
    char m_Url[MAX_PATH];           // 文件路径
    volatile int m_AbortRequest = 0;       // 退出标志
    volatile int m_PauseRequest = 0;       // 暂停标志

    double m_StartTime = 0;        // 播放起始时间 s
    double m_Duration  = 0;        // 播放总时长单位 s
    int64_t m_CurTimestamp = 0;     // 当前播放视频或音频位置 ms
    int64_t m_SysTimeBase  = 0;     // 系统时钟的对齐时间 ms

    //seek position
    volatile int m_SeekRequest = 0; // Seek 请求
    volatile int m_SeekSuccess = 0; // Seek 标志
    int64_t m_SeekPosition = 0;     // Seek 位置

    //play mode
    int m_AutoExit = 0;             // 自动退出
    int m_Loop     = 1;             // 循环播放
};


#endif //LEARNFFMPEG_PLAYERSTATE_H

#ifndef FRAME_SHM_READER_H
#define FRAME_SHM_READER_H

#include "ipc/frame_shm.h"

#include <opencv2/core.hpp>

// 帧 shm 只读端：shm_open + mmap，按 seqlock 拷贝最新完整帧到 cv::Mat。
// Linux 板端使用；Windows 上 open/copyLatest 恒失败（无 POSIX shm）。
class FrameShmReader
{
public:
    FrameShmReader();
    ~FrameShmReader();

    bool open();
    void close();
    bool copyLatest(cv::Mat &out);

private:
    int m_fd = -1;
    DmsFrameShmLayout *m_shm = nullptr;
};

#endif // FRAME_SHM_READER_H

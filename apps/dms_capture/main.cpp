// dms_capture：板上唯一开相机的进程。
// 流程：V4L2 取帧 → 必要时缩放到 shm 上限 → seqlock 写入 /dms_frame_shm
// 启动顺序建议：本进程 → dms_ai → SmartCockpitDMS（读者依赖 shm 已创建）
#include "ipc/frame_shm.h"
#include "capture/v4l2capture.h"

#include <opencv2/imgproc.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <thread>

#include <QString>

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

namespace {

#ifdef Q_OS_LINUX

std::atomic_bool g_running{true};

void onSignal(int)
{
    g_running.store(false);
}

uint64_t nowNs()
{
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull
         + static_cast<uint64_t>(ts.tv_nsec);
}

void publishFrame(DmsFrameShmLayout *shm, const cv::Mat &bgr)
{
    // seqlock：先奇数（写中）→ 写像素/元数据 → 再偶数（可读）；读者见 frame_shm.h
    if (!shm || bgr.empty() || bgr.type() != CV_8UC3) {
        return;
    }
    if (bgr.cols > static_cast<int>(kDmsFrameMaxW)
        || bgr.rows > static_cast<int>(kDmsFrameMaxH)) {
        return;
    }

    const uint32_t bytes = static_cast<uint32_t>(bgr.cols * bgr.rows * 3);

    // seqlock：先标奇数（写中）→ 写 data → 再标偶数（可读）
    const uint64_t seq = __atomic_load_n(&shm->hdr.seq, __ATOMIC_RELAXED);
    __atomic_store_n(&shm->hdr.seq, seq + 1, __ATOMIC_RELEASE);

    shm->hdr.width = static_cast<uint32_t>(bgr.cols);
    shm->hdr.height = static_cast<uint32_t>(bgr.rows);
    shm->hdr.format = DmsFrameFormatBgr888;
    shm->hdr.data_size = bytes;
    shm->hdr.timestamp_ns = nowNs();

    if (bgr.isContinuous()) {
        std::memcpy(shm->data, bgr.data, bytes);
    } else {
        for (int r = 0; r < bgr.rows; ++r) {
            std::memcpy(shm->data + r * bgr.cols * 3,
                        bgr.ptr(r),
                        static_cast<size_t>(bgr.cols * 3));
        }
    }

    __atomic_store_n(&shm->hdr.seq, seq + 2, __ATOMIC_RELEASE);
}

#endif // Q_OS_LINUX

} // namespace

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

#ifndef Q_OS_LINUX
    std::fprintf(stderr, "dms_capture is for Linux/RK board only (POSIX shm + V4L2).\n");
    return 1;
#else
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    const int fd = ::shm_open(kDmsFrameShmName, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        std::perror("shm_open");
        return 1;
    }

    if (::ftruncate(fd, static_cast<off_t>(kDmsFrameShmBytes)) != 0) {
        std::perror("ftruncate");
        ::close(fd);
        return 1;
    }

    void *map = ::mmap(nullptr,
                       kDmsFrameShmBytes,
                       PROT_READ | PROT_WRITE,
                       MAP_SHARED,
                       fd,
                       0);
    if (map == MAP_FAILED) {
        std::perror("mmap");
        ::close(fd);
        return 1;
    }

    auto *shm = static_cast<DmsFrameShmLayout *>(map);
    std::memset(shm, 0, kDmsFrameShmBytes);
    shm->hdr.magic = kDmsFrameShmMagic;
    __atomic_store_n(&shm->hdr.seq, 0, __ATOMIC_RELAXED);

    V4l2Capture cap;
    const char *devs[] = {
        "/dev/video11", "/dev/video0", "/dev/video9",
        "/dev/video10", "/dev/video1"
    };
    bool opened = false;
    for (const char *dev : devs) {
        if (cap.open(QString::fromLatin1(dev), 320, 240)) {
            std::printf("[dms_capture] V4L2 ok %s %dx%d\n",
                        dev, cap.width(), cap.height());
            opened = true;
            break;
        }
    }
    if (!opened) {
        std::fprintf(stderr, "[dms_capture] open camera failed\n");
        ::munmap(map, kDmsFrameShmBytes);
        ::close(fd);
        return 1;
    }

    std::printf("[dms_capture] writing shm %s (%zu bytes), Ctrl+C to stop\n",
                kDmsFrameShmName, kDmsFrameShmBytes);

    while (g_running.load()) {
        cv::Mat frame;
        if (!cap.read(frame) || frame.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        cv::Mat bgr = frame;
        if (bgr.cols > static_cast<int>(kDmsFrameMaxW)
            || bgr.rows > static_cast<int>(kDmsFrameMaxH)) {
            cv::resize(frame, bgr, cv::Size(320, 240));
        }

        publishFrame(shm, bgr);
    }

    std::printf("[dms_capture] stopping\n");
    cap.close();
    ::munmap(map, kDmsFrameShmBytes);
    ::close(fd);
    // 不 shm_unlink：方便读者进程仍能打开同名对象；需要清名时手动 unlink
    return 0;
#endif
}

#include "ipc/frame_shm_reader.h"

#include <cstring>
#include <vector>

#include <opencv2/core.hpp>
#include <QtGlobal>

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#endif

FrameShmReader::FrameShmReader() = default;

FrameShmReader::~FrameShmReader()
{
    close();
}

bool FrameShmReader::open()
{
#ifndef Q_OS_LINUX
    return false;
#else
    if (m_shm) {
        return true;
    }

    const int fd = ::shm_open(kDmsFrameShmName, O_RDONLY, 0666);
    if (fd < 0) {
        std::perror("shm_open");
        return false;
    }

    void *map = ::mmap(nullptr, kDmsFrameShmBytes, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        std::perror("mmap");
        ::close(fd);
        return false;
    }

    m_fd = fd;
    m_shm = static_cast<DmsFrameShmLayout *>(map);

    if (m_shm->hdr.magic != 0 && m_shm->hdr.magic != kDmsFrameShmMagic) {
        close();
        return false;
    }
    return true;
#endif
}

void FrameShmReader::close()
{
#ifdef Q_OS_LINUX
    if (m_shm) {
        ::munmap(m_shm, kDmsFrameShmBytes);
        m_shm = nullptr;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif
}

bool FrameShmReader::copyLatest(cv::Mat &out)
{
#ifndef Q_OS_LINUX
    (void)out;
    return false;
#else
    if (!m_shm) {
        return false;
    }

    for (int attempt = 0; attempt < 8; ++attempt) {
        const uint64_t seq1 =
            __atomic_load_n(&m_shm->hdr.seq, __ATOMIC_ACQUIRE);
        if (seq1 & 1ull) {
            continue;
        }

        const uint32_t w = m_shm->hdr.width;
        const uint32_t h = m_shm->hdr.height;
        const uint32_t n = m_shm->hdr.data_size;

        if (w == 0 || h == 0
            || w > kDmsFrameMaxW || h > kDmsFrameMaxH
            || n != w * h * 3u
            || n > kDmsFrameMaxBytes) {
            continue;
        }

        std::vector<uint8_t> tmp(n);
        std::memcpy(tmp.data(), m_shm->data, n);

        const uint64_t seq2 =
            __atomic_load_n(&m_shm->hdr.seq, __ATOMIC_ACQUIRE);
        if (seq1 != seq2 || (seq2 & 1ull)) {
            continue;
        }

        out.create(static_cast<int>(h), static_cast<int>(w), CV_8UC3);
        std::memcpy(out.data, tmp.data(), n);
        return true;
    }

    return false;
#endif
}

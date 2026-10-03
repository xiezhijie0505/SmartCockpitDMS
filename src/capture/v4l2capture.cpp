#include "v4l2capture.h"

#include <QDebug>
#include <cstring>

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <vector>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#endif

V4l2Capture::V4l2Capture()
{
#ifdef Q_OS_LINUX
    memset(m_buffers, 0, sizeof(m_buffers));
#endif
}

V4l2Capture::~V4l2Capture()
{
    close();
}

bool V4l2Capture::isOpened() const
{
    return m_opened;
}

#ifndef Q_OS_LINUX

bool V4l2Capture::open(const QString &, int, int)
{
    return false;
}

bool V4l2Capture::read(cv::Mat &)
{
    return false;
}

void V4l2Capture::close()
{
    m_opened = false;
    m_fd = -1;
}

#else

int V4l2Capture::xioctl(int request, void *arg)
{
    int r;
    do {
        r = ::ioctl(m_fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

bool V4l2Capture::frameToBgr(int index, cv::Mat &bgrOut)
{
    const Buffer &buf = m_buffers[index];
    if (!buf.start[0]) {
        return false;
    }

    if (m_pixFmt == V4L2_PIX_FMT_YUYV) {
        cv::Mat yuyv(m_height, m_width, CV_8UC2, buf.start[0]);
        cv::cvtColor(yuyv, bgrOut, cv::COLOR_YUV2BGR_YUY2);
        return !bgrOut.empty();
    }

    if (m_pixFmt == V4L2_PIX_FMT_NV12 || m_pixFmt == V4L2_PIX_FMT_NV21) {
        cv::Mat yuv(m_height * 3 / 2, m_width, CV_8UC1, buf.start[0]);
        const int code = (m_pixFmt == V4L2_PIX_FMT_NV12)
                             ? cv::COLOR_YUV2BGR_NV12
                             : cv::COLOR_YUV2BGR_NV21;
        cv::cvtColor(yuv, bgrOut, code);
        return !bgrOut.empty();
    }

    if (m_pixFmt == V4L2_PIX_FMT_MJPEG) {
        std::vector<uchar> jpeg(
            static_cast<uchar *>(buf.start[0]),
            static_cast<uchar *>(buf.start[0]) + buf.length[0]);
        bgrOut = cv::imdecode(jpeg, cv::IMREAD_COLOR);
        return !bgrOut.empty();
    }

    qWarning() << "[V4l2Capture] unsupported format" << m_pixFmt;
    return false;
}

bool V4l2Capture::open(const QString &device, int width, int height)
{
    close();

    m_fd = ::open(device.toLocal8Bit().constData(), O_RDWR);
    if (m_fd < 0) {
        qWarning() << "[V4l2Capture] open device failed" << device;
        return false;
    }

    const unsigned int pixList[] = {
        V4L2_PIX_FMT_NV12,
        V4L2_PIX_FMT_NV21,
        V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_MJPEG,
    };
    const int sizes[][2] = {
        {width, height},
        {640, 480},
        {320, 240},
        {1280, 720},
    };

    bool ok = false;
    for (unsigned int pix : pixList) {
        for (const auto &sz : sizes) {
            v4l2_format fmt;
            memset(&fmt, 0, sizeof(fmt));
            fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            fmt.fmt.pix_mp.width = sz[0];
            fmt.fmt.pix_mp.height = sz[1];
            fmt.fmt.pix_mp.pixelformat = pix;
            fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
            fmt.fmt.pix_mp.num_planes = 1;
            if (xioctl(VIDIOC_S_FMT, &fmt) == 0
                && fmt.fmt.pix_mp.pixelformat == pix) {
                m_useMplane = true;
                m_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
                m_pixFmt = pix;
                m_width = fmt.fmt.pix_mp.width;
                m_height = fmt.fmt.pix_mp.height;
                ok = true;
                break;
            }
        }
        if (ok) {
            break;
        }
    }

    if (!ok) {
        for (unsigned int pix : pixList) {
            for (const auto &sz : sizes) {
                v4l2_format fmt;
                memset(&fmt, 0, sizeof(fmt));
                fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                fmt.fmt.pix.width = sz[0];
                fmt.fmt.pix.height = sz[1];
                fmt.fmt.pix.pixelformat = pix;
                fmt.fmt.pix.field = V4L2_FIELD_NONE;
                if (xioctl(VIDIOC_S_FMT, &fmt) == 0
                    && fmt.fmt.pix.pixelformat == pix) {
                    m_useMplane = false;
                    m_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                    m_pixFmt = pix;
                    m_width = fmt.fmt.pix.width;
                    m_height = fmt.fmt.pix.height;
                    ok = true;
                    break;
                }
            }
            if (ok) {
                break;
            }
        }
    }

    if (!ok) {
        qWarning() << "[V4l2Capture] S_FMT failed";
        close();
        return false;
    }

    // 请求 30fps（驱动可不支持；以 G_PARM 实际值为准）
    {
        v4l2_streamparm parm;
        memset(&parm, 0, sizeof(parm));
        parm.type = m_type;
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = 30;
        if (xioctl(VIDIOC_S_PARM, &parm) == 0
            && xioctl(VIDIOC_G_PARM, &parm) == 0
            && parm.parm.capture.timeperframe.numerator > 0) {
            const double fps = static_cast<double>(parm.parm.capture.timeperframe.denominator)
                               / parm.parm.capture.timeperframe.numerator;
            qDebug() << "[V4l2Capture] negotiated fps" << fps;
        } else {
            qWarning() << "[V4l2Capture] S_PARM fps not applied (driver may fix rate)";
        }
    }

    v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = kBufCount;
    req.type = m_type;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(VIDIOC_REQBUFS, &req) < 0) {
        close();
        return false;
    }

    for (int i = 0; i < kBufCount; ++i) {
        v4l2_buffer b;
        v4l2_plane planes[VIDEO_MAX_PLANES];
        memset(&b, 0, sizeof(b));
        memset(planes, 0, sizeof(planes));
        b.type = m_type;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;
        if (m_useMplane) {
            b.length = VIDEO_MAX_PLANES;
            b.m.planes = planes;
        }
        if (xioctl(VIDIOC_QUERYBUF, &b) < 0) {
            close();
            return false;
        }

        if (m_useMplane) {
            m_buffers[i].nplanes = static_cast<int>(b.length);
            for (int p = 0; p < m_buffers[i].nplanes; ++p) {
                m_buffers[i].length[p] = planes[p].length;
                m_buffers[i].start[p] = mmap(
                    nullptr, planes[p].length,
                    PROT_READ | PROT_WRITE, MAP_SHARED,
                    m_fd, planes[p].m.mem_offset);
                if (m_buffers[i].start[p] == MAP_FAILED) {
                    close();
                    return false;
                }
            }
        } else {
            m_buffers[i].nplanes = 1;
            m_buffers[i].length[0] = b.length;
            m_buffers[i].start[0] = mmap(
                nullptr, b.length,
                PROT_READ | PROT_WRITE, MAP_SHARED,
                m_fd, b.m.offset);
            if (m_buffers[i].start[0] == MAP_FAILED) {
                close();
                return false;
            }
        }

        if (xioctl(VIDIOC_QBUF, &b) < 0) {
            close();
            return false;
        }
    }

    enum v4l2_buf_type type = m_type;
    if (xioctl(VIDIOC_STREAMON, &type) < 0) {
        close();
        return false;
    }

    m_device = device;
    m_opened = true;
    qDebug() << "[V4l2Capture] ok" << device << m_width << "x" << m_height;
    return true;
}

bool V4l2Capture::read(cv::Mat &bgrOut)
{
    if (!m_opened || m_fd < 0) {
        return false;
    }

    v4l2_buffer b;
    v4l2_plane planes[VIDEO_MAX_PLANES];
    memset(&b, 0, sizeof(b));
    memset(planes, 0, sizeof(planes));
    b.type = m_type;
    b.memory = V4L2_MEMORY_MMAP;
    if (m_useMplane) {
        b.length = VIDEO_MAX_PLANES;
        b.m.planes = planes;
    }

    if (xioctl(VIDIOC_DQBUF, &b) < 0) {
        return false;
    }

    const bool ok = frameToBgr(static_cast<int>(b.index), bgrOut);

    if (xioctl(VIDIOC_QBUF, &b) < 0) {
        qWarning() << "[V4l2Capture] QBUF failed";
        return false;
    }
    return ok;
}

void V4l2Capture::close()
{
    if (m_fd >= 0) {
        if (m_opened) {
            enum v4l2_buf_type type = m_type;
            xioctl(VIDIOC_STREAMOFF, &type);
        }
        for (int i = 0; i < kBufCount; ++i) {
            for (int p = 0; p < m_buffers[i].nplanes; ++p) {
                if (m_buffers[i].start[p]
                    && m_buffers[i].start[p] != MAP_FAILED) {
                    munmap(m_buffers[i].start[p], m_buffers[i].length[p]);
                }
                m_buffers[i].start[p] = nullptr;
                m_buffers[i].length[p] = 0;
            }
            m_buffers[i].nplanes = 0;
        }
        ::close(m_fd);
        m_fd = -1;
    }
    m_opened = false;
}

#endif // Q_OS_LINUX

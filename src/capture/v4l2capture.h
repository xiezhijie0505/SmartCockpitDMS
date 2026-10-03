#ifndef V4L2CAPTURE_H
#define V4L2CAPTURE_H

#include <QString>
#include <opencv2/core.hpp>

#ifdef Q_OS_LINUX
#include <linux/videodev2.h>
#endif

class V4l2Capture
{
public:
    static const int kBufCount = 4;

    V4l2Capture();
    ~V4l2Capture();

    bool open(const QString &device, int width, int height);
    bool isOpened() const;
    bool read(cv::Mat &bgrOut);
    void close();

    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    bool m_opened = false;
    int m_fd = -1;
    QString m_device;
    int m_width = 0;
    int m_height = 0;

#ifdef Q_OS_LINUX
    struct Buffer {
        void *start[VIDEO_MAX_PLANES];
        size_t length[VIDEO_MAX_PLANES];
        int nplanes;
    };

    bool frameToBgr(int index, cv::Mat &bgrOut);
    int xioctl(int request, void *arg);

    Buffer m_buffers[kBufCount];
    enum v4l2_buf_type m_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    unsigned int m_pixFmt = 0;
    bool m_useMplane = false;
#endif
};

#endif // V4L2CAPTURE_H

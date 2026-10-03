/*
 * V4L2 练习（适配 RK3568 rkisp）
 *
 * Rockchip ISP 常用「多平面」接口：V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
 * USB 摄像头才常用普通 VIDEO_CAPTURE + MJPEG。
 *
 * 交叉编译：
 *   make clean
 *   make CROSS=aarch64-linux-gnu-
 *   file v4l2_capture          # 必须显示 ARM aarch64
 *
 * 板子：
 *   ./v4l2_capture
 *   ./v4l2_capture /dev/video0
 *   ./v4l2_capture /dev/video11
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define BUF_CNT 4

struct buffer {
    void  *start[VIDEO_MAX_PLANES];
    size_t length[VIDEO_MAX_PLANES];
    int    nplanes;
};

static int cam_cmd(int fd, int cmd, void *arg)
{
    int r;
    do {
        r = ioctl(fd, cmd, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

static const char *fcc(unsigned int f, char s[5])
{
    s[0] = (char)(f & 0xFF);
    s[1] = (char)((f >> 8) & 0xFF);
    s[2] = (char)((f >> 16) & 0xFF);
    s[3] = (char)((f >> 24) & 0xFF);
    s[4] = 0;
    return s;
}

static void list_formats(int fd, enum v4l2_buf_type type)
{
    struct v4l2_fmtdesc desc;
    char s[5];
    int i;
    printf("   支持的格式 (type=%d):\n", (int)type);
    for (i = 0; ; i++) {
        memset(&desc, 0, sizeof(desc));
        desc.index = i;
        desc.type = type;
        if (cam_cmd(fd, VIDIOC_ENUM_FMT, &desc) < 0)
            break;
        printf("     %s  %s\n", fcc(desc.pixelformat, s), desc.description);
    }
}

static int try_set_fmt_mplane(int fd, unsigned int pix, int w, int h,
                              struct v4l2_format *out)
{
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = w;
    fmt.fmt.pix_mp.height = h;
    fmt.fmt.pix_mp.pixelformat = pix;
    fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
    fmt.fmt.pix_mp.num_planes = 1;
    if (cam_cmd(fd, VIDIOC_S_FMT, &fmt) < 0)
        return -1;
    if (fmt.fmt.pix_mp.pixelformat != pix)
        return -1;
    *out = fmt;
    return 0;
}

static int try_set_fmt_single(int fd, unsigned int pix, int w, int h,
                              struct v4l2_format *out)
{
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = w;
    fmt.fmt.pix.height = h;
    fmt.fmt.pix.pixelformat = pix;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (cam_cmd(fd, VIDIOC_S_FMT, &fmt) < 0)
        return -1;
    if (fmt.fmt.pix.pixelformat != pix)
        return -1;
    *out = fmt;
    return 0;
}

static void save_gray_ppm(const char *path, const unsigned char *y,
                          int width, int height)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror("fopen");
        return;
    }
    fprintf(fp, "P5\n%d %d\n255\n", width, height);
    fwrite(y, 1, (size_t)width * (size_t)height, fp);
    fclose(fp);
    printf("6) 已保存 %s（灰度，可用看图软件打开）\n", path);
}

int main(int argc, char **argv)
{
    const char *dev = "/dev/video0";
    int fd, i, frame_no = 0;
    int use_mplane = 0;
    int width = 0, height = 0;
    unsigned int pixfmt = 0;
    char s[5];
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct buffer buffers[BUF_CNT];
    enum v4l2_buf_type type;
    int ok = 0;

    unsigned int pix_list[] = {
        V4L2_PIX_FMT_NV12,
        V4L2_PIX_FMT_NV21,
        V4L2_PIX_FMT_NV16,
        V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_MJPEG,
    };
    int sizes[][2] = { {640, 480}, {1280, 720}, {320, 240}, {1920, 1080} };

    if (argc >= 2)
        dev = argv[1];

    printf(">>> 若仍看到「可改成别的摄像头再试」，说明板子上还是旧程序，请重新拷贝新 v4l2_capture\n");

    fd = open(dev, O_RDWR);
    if (fd < 0) {
        printf("打不开 %s，先: ls /dev/video*\n", dev);
        return 1;
    }
    printf("1) 已打开 %s\n", dev);

    memset(&cap, 0, sizeof(cap));
    cam_cmd(fd, VIDIOC_QUERYCAP, &cap);
    printf("2) 设备名: %s  driver: %s\n", cap.card, cap.driver);
    printf("   capabilities=0x%x\n", cap.device_caps ? cap.device_caps : cap.capabilities);

    list_formats(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    list_formats(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);

    /* 先试 MPLANE（RK ISP），再试单平面（USB） */
    for (i = 0; i < (int)(sizeof(pix_list) / sizeof(pix_list[0])) && !ok; i++) {
        int si;
        for (si = 0; si < (int)(sizeof(sizes) / sizeof(sizes[0])); si++) {
            if (try_set_fmt_mplane(fd, pix_list[i], sizes[si][0], sizes[si][1], &fmt) == 0) {
                use_mplane = 1;
                pixfmt = fmt.fmt.pix_mp.pixelformat;
                width = fmt.fmt.pix_mp.width;
                height = fmt.fmt.pix_mp.height;
                ok = 1;
                printf("3) MPLANE 格式 OK: %dx%d %s planes=%d\n",
                       width, height, fcc(pixfmt, s), fmt.fmt.pix_mp.num_planes);
                break;
            }
        }
    }
    for (i = 0; i < (int)(sizeof(pix_list) / sizeof(pix_list[0])) && !ok; i++) {
        int si;
        for (si = 0; si < (int)(sizeof(sizes) / sizeof(sizes[0])); si++) {
            if (try_set_fmt_single(fd, pix_list[i], sizes[si][0], sizes[si][1], &fmt) == 0) {
                use_mplane = 0;
                pixfmt = fmt.fmt.pix.pixelformat;
                width = fmt.fmt.pix.width;
                height = fmt.fmt.pix.height;
                ok = 1;
                printf("3) 单平面格式 OK: %dx%d %s\n", width, height, fcc(pixfmt, s));
                break;
            }
        }
    }

    if (!ok) {
        printf("设置格式失败。请把上面全部输出发我，并执行: ls -l /dev/video*\n");
        close(fd);
        return 1;
    }

    memset(&req, 0, sizeof(req));
    req.count = BUF_CNT;
    req.memory = V4L2_MEMORY_MMAP;
    req.type = use_mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
                          : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (cam_cmd(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("VIDIOC_REQBUFS");
        close(fd);
        return 1;
    }

    for (i = 0; i < BUF_CNT; i++) {
        struct v4l2_buffer b;
        struct v4l2_plane planes[VIDEO_MAX_PLANES];
        int p;

        memset(&b, 0, sizeof(b));
        memset(planes, 0, sizeof(planes));
        b.type = req.type;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;

        if (use_mplane) {
            b.length = VIDEO_MAX_PLANES;
            b.m.planes = planes;
        }

        if (cam_cmd(fd, VIDIOC_QUERYBUF, &b) < 0) {
            perror("VIDIOC_QUERYBUF");
            close(fd);
            return 1;
        }

        if (use_mplane) {
            buffers[i].nplanes = (int)b.length;
            for (p = 0; p < buffers[i].nplanes; p++) {
                buffers[i].length[p] = planes[p].length;
                buffers[i].start[p] = mmap(NULL, planes[p].length,
                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, planes[p].m.mem_offset);
                if (buffers[i].start[p] == MAP_FAILED) {
                    perror("mmap");
                    close(fd);
                    return 1;
                }
            }
        } else {
            buffers[i].nplanes = 1;
            buffers[i].length[0] = b.length;
            buffers[i].start[0] = mmap(NULL, b.length,
                PROT_READ | PROT_WRITE, MAP_SHARED, fd, b.m.offset);
            if (buffers[i].start[0] == MAP_FAILED) {
                perror("mmap");
                close(fd);
                return 1;
            }
        }

        if (cam_cmd(fd, VIDIOC_QBUF, &b) < 0) {
            perror("VIDIOC_QBUF");
            close(fd);
            return 1;
        }
    }
    printf("4) 缓冲区准备好\n");

    type = req.type;
    if (cam_cmd(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
        close(fd);
        return 1;
    }
    printf("5) 开始要图...\n");

    while (frame_no < 15) {
        struct v4l2_buffer b;
        struct v4l2_plane planes[VIDEO_MAX_PLANES];

        memset(&b, 0, sizeof(b));
        memset(planes, 0, sizeof(planes));
        b.type = req.type;
        b.memory = V4L2_MEMORY_MMAP;
        if (use_mplane) {
            b.length = VIDEO_MAX_PLANES;
            b.m.planes = planes;
        }

        if (cam_cmd(fd, VIDIOC_DQBUF, &b) < 0) {
            usleep(30000);
            continue;
        }

        frame_no++;
        printf("   第 %d 帧\n", frame_no);

        if (frame_no == 15) {
            /* NV12：第一平面前 width*height 是 Y，可当灰度看 */
            const unsigned char *y = (const unsigned char *)buffers[b.index].start[0];
            save_gray_ppm("frame_gray.ppm", y, width, height);
        }

        if (cam_cmd(fd, VIDIOC_QBUF, &b) < 0) {
            perror("VIDIOC_QBUF");
            break;
        }
    }

    type = req.type;
    cam_cmd(fd, VIDIOC_STREAMOFF, &type);
    for (i = 0; i < BUF_CNT; i++) {
        int p;
        for (p = 0; p < buffers[i].nplanes; p++)
            munmap(buffers[i].start[p], buffers[i].length[p]);
    }
    close(fd);

    printf("7) 结束。把 frame_gray.ppm 拷到电脑打开。\n");
    printf("   若开头没有「MPLANE 格式 OK / 单平面格式 OK」，说明跑的还是旧文件。\n");
    return 0;
}

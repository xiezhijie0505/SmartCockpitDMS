/*
 * ============================================================================
 * 实验 01：摄像头预览采集（对应简历「摄像头预览采集 / 板端显示联调」）
 * ============================================================================
 *
 * 【你要达成的功能】
 *   打开摄像头 → 持续读帧 → 窗口里看到自己 → 按 s 保存一张图 → 按 q 退出
 *
 * 【做完你应能说清】
 *   1. VideoCapture 打开的是哪个设备索引
 *   2. 为什么要设分辨率 / 帧率 / 缓冲
 *   3. 预览循环长什么样（和打卡页 QTimer 读帧是同一类事）
 *
 * 【怎么编译运行】（Windows 已装 OpenCV 时）
 *   g++ 01_preview.cpp -o 01_preview `pkg-config --cflags --libs opencv4`
 *   或在你熟悉的 OpenCV 工程里把本文件加进去跑
 *   运行：./01_preview
 *   若打不开，改下面 kCameraIndex（板子上 C270 常试 9）
 *
 * 【对照你的正式项目】
 *   src/views/attendanceclockview.cpp 里 startCamera() / updateFrame()
 * ============================================================================
 */

#include <opencv2/opencv.hpp>
#include <iostream>
#include <cstdio>

using namespace cv;
using namespace std;

// ---------- 和正式项目接近的参数（易错点都标在注释里）----------
static const int kCameraIndex = 0;   // Windows 常是 0；RK 上 USB 可能是 9
static const int kWidth       = 320; // 越大越清晰，但越卡
static const int kHeight      = 240;
static const int kFps         = 15;
static const int kBufferSize  = 1;   // 1=尽量要「最新帧」，减少延迟

int main()
{
    cout << "=== 实验01：预览采集 ===\n";
    cout << "目标：窗口里看到画面；按 s 拍照；按 q 退出\n\n";

    // ========== 步骤1：打开摄像头 ==========
    // 难点：索引不一定是 0；UVC 还可能有「不能出图」的附属节点
    VideoCapture cap(kCameraIndex);
    if (!cap.isOpened()) {
        cerr << "[失败] 打不开摄像头 index=" << kCameraIndex << "\n";
        cerr << "易错点：换 1/2/9 再试；确认没有别的程序占用摄像头\n";
        return 1;
    }
    cout << "[OK] 摄像头已打开 index=" << kCameraIndex << "\n";

    // ========== 步骤2：设采集参数（边缘优化会动这里）==========
    // 正式项目同样设置了 MJPG / 320x240 / 15 / buffersize=1
    cap.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(CAP_PROP_FRAME_WIDTH,  kWidth);
    cap.set(CAP_PROP_FRAME_HEIGHT, kHeight);
    cap.set(CAP_PROP_FPS,         kFps);
    cap.set(CAP_PROP_BUFFERSIZE,  kBufferSize);

    // 读回实际值（很多摄像头会「假装接受」，实际另一套分辨率）
    double realW = cap.get(CAP_PROP_FRAME_WIDTH);
    double realH = cap.get(CAP_PROP_FRAME_HEIGHT);
    double realF = cap.get(CAP_PROP_FPS);
    cout << "[信息] 请求 " << kWidth << "x" << kHeight << "@" << kFps
         << "  实际约 " << realW << "x" << realH << "@" << realF << "\n";
    cout << "易错点：set 成功不等于一定生效，一定要用 get 核对\n\n";

    namedWindow("preview", WINDOW_NORMAL);

    // ========== 步骤3：预览循环 =「帧处理」最简单形式 ==========
    // 正式项目：QTimer 触发 updateFrame()，本质也是 while 里 read
    Mat frame;
    int frameCount = 0;
    cout << "操作：窗口激活后  s=保存一帧  q=退出\n";

    while (true) {
        // --- 采集：要到一帧 ---
        // 难点/易错：有的驱动会积压旧帧，可先 grab 丢掉再 read（正式项目也有丢帧逻辑）
        if (!cap.read(frame) || frame.empty()) {
            cerr << "[警告] 读帧失败（设备掉了？被占用？）\n";
            break;
        }
        frameCount++;

        // --- 帧处理（这里只做显示+叠字；正式项目还会检测/活体/识别）---
        putText(frame,
                format("frames=%d  %dx%d", frameCount, frame.cols, frame.rows),
                Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 0), 2);

        imshow("preview", frame);

        // 等按键：1ms 轮询，保证画面流畅
        int key = waitKey(1) & 0xFF;
        if (key == 'q' || key == 27) {
            cout << "[结束] 退出预览，共 " << frameCount << " 帧\n";
            break;
        }
        if (key == 's') {
            // --- 采集结果落盘：证明「采到了可用的一帧」---
            const char *path = "lab01_capture.jpg";
            if (imwrite(path, frame)) {
                cout << "[OK] 已保存 " << path
                     << "  （对应：能采；正式项目会拿这帧去提特征）\n";
            } else {
                cerr << "[失败] 保存图片失败\n";
            }
        }
    }

    // ========== 步骤4：释放（正式项目 stopCamera 也会 release）==========
    // 易错点：不 release，其它页面/程序会打不开同一摄像头
    cap.release();
    destroyAllWindows();

    cout << "\n=== 本实验你要记住的 3 点 ===\n";
    cout << "1. 预览=循环 read 帧 + 显示\n";
    cout << "2. 分辨率/缓冲直接影响卡顿和延迟（边缘优化就调这些）\n";
    cout << "3. 用完必须 release，否则抢设备\n";
    return 0;
}

/*
 * ---------------------------------------------------------------------------
 * 【本实验对应简历哪一句】
 *   「摄像头预览采集」「板端摄像头显示联调」
 *
 * 【做好哪一点】
 *   - 稳定出画（索引对、没被占用）
 *   - 参数可解释（为何 320x240、buffer=1）
 *   - 资源释放干净
 *
 * 【易错点】
 *   1. 摄像头索引错（板子上 0 可能是 rkisp，USB 在 9）
 *   2. 两个窗口同时 open 同一 UVC → 第二个失败
 *   3. set 分辨率后不 get 核对
 *   4. 缓冲太大 → 画面「慢半拍」
 *
 * 【比较难点】
 *   1. 板端节点多，要会排查哪个 video 能出图
 *   2. 预览要流畅、识别又吃 CPU，要拆成「显示定时器 + 识别定时器」
 *      （正式项目：33ms 刷新显示，200ms 做一次识别）
 *   3. 延迟优化：buffer=1、必要时丢旧帧
 * ---------------------------------------------------------------------------
 */

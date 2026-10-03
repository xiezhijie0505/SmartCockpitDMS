#ifndef FRAME_SHM_H
#define FRAME_SHM_H

#include <cstdint>
#include <cstddef>

// =============================================================================
// DMS 帧共享内存契约（S4.2）
//
// 进程角色：
//   - 唯一写者：dms_capture（V4L2 取帧后写入）
//   - 读者：dms_ai（推理）、HMI/SmartCockpitDMS（预览）、人脸采集对话框（可选）
//
// 策略：只保留最新一帧（latest-wins），允许丢弃中间帧。
//
// 写序（seqlock）：
//   1) 将 hdr.seq 变为奇数（表示写中）
//   2) 写入 width/height/format/data_size/timestamp_ns 与 data[]
//   3) 最后将 hdr.seq 变为偶数（表示本帧完整可读）
//
// 读序：
//   1) 读 seq1，若为奇数则重试
//   2) 按 data_size 拷贝 data[]（以及宽高等元数据）到本地
//   3) 再读 seq2；若 seq1 != seq2 或 seq2 为奇数，则丢弃重试
//
// 像素约定：BGR888，行优先，连续存放；单帧不超过 MaxW x MaxH。
// =============================================================================

// C++11：用 static constexpr（不要 inline，那是 C++17）
static constexpr const char kDmsFrameShmName[] = "/dms_frame_shm";

static constexpr uint32_t kDmsFrameShmMagic = 0x444D5346u; // 'DMSF'

static constexpr uint32_t kDmsFrameMaxW = 640;
static constexpr uint32_t kDmsFrameMaxH = 480;
static constexpr uint32_t kDmsFrameMaxBytes = kDmsFrameMaxW * kDmsFrameMaxH * 3;

enum DmsFrameFormat : uint32_t {
    DmsFrameFormatBgr888 = 0
};

struct DmsFrameShmHeader {
    uint32_t magic;
    uint64_t seq;          // 偶=可读，奇=写中
    uint64_t timestamp_ns;
    uint32_t width;
    uint32_t height;
    uint32_t format;       // DmsFrameFormat
    uint32_t data_size;    // 通常为 width * height * 3
};

struct DmsFrameShmLayout {
    DmsFrameShmHeader hdr;
    uint8_t data[kDmsFrameMaxBytes];
};

static constexpr std::size_t kDmsFrameShmBytes = sizeof(DmsFrameShmLayout);

static_assert(sizeof(DmsFrameShmHeader) == 40,
              "DmsFrameShmHeader size changed; check packing across processes");
static_assert(kDmsFrameShmBytes == sizeof(DmsFrameShmHeader) + kDmsFrameMaxBytes,
              "DmsFrameShmLayout must be header + raw pixel buffer");

#endif // FRAME_SHM_H

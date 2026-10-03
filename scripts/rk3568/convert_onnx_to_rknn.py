#!/usr/bin/env python3
"""
在 Ubuntu(x86) 上把 YuNet / SFace 的 ONNX 转成 RK3568 用的 .rknn

依赖（任选其一）:
  pip install rknn-toolkit2
  或按官方文档用 Docker / whl:
  https://github.com/airockchip/rknn-toolkit2

用法:
  python3 convert_onnx_to_rknn.py \\
    --detect models/face_detection_yunet_2022mar.onnx \\
    --recog  models/face_recognition_sface_2021dec.onnx \\
    --out    models/rknn

说明:
  - 先默认 FP（do_quantization=False），更容易和 OpenCV 结果对齐
  - 检测输入固定为 320x240（与板上预览分辨率一致）
  - 识别输入固定为 112x112（SFace 常规）
  - 转完 .rknn 后，程序仍须改成 RKNN Runtime 推理才会走 NPU
"""
from __future__ import annotations

import argparse
import os
import sys


def convert_one(
    onnx_path: str,
    rknn_path: str,
    input_size: tuple[int, int],
    mean_values,
    std_values,
    do_quantization: bool,
    dataset: str | None,
):
    from rknn.api import RKNN

    if not os.path.isfile(onnx_path):
        raise FileNotFoundError(onnx_path)

    w, h = input_size
    rknn = RKNN(verbose=True)
    print(f"\n======== convert {onnx_path} -> {rknn_path} ========")
    print(f"input WxH = {w}x{h}, quant={do_quantization}")

    # mean/std 需与后续板上预处理一致；FP 先用 [0]/[255] 常见于 OpenCV DNN BGR 输入
    rknn.config(
        mean_values=mean_values,
        std_values=std_values,
        target_platform="rk3568",
    )

    # 先按固定尺寸加载；失败则退回默认（看工具输出再改尺寸/输入名）
    ret = rknn.load_onnx(model=onnx_path, input_size_list=[[1, 3, h, w]])
    if ret != 0:
        print("load_onnx with input_size_list failed, retry plain load_onnx...")
        ret = rknn.load_onnx(model=onnx_path)
    if ret != 0:
        raise RuntimeError(f"load_onnx failed: {ret}")

    if do_quantization:
        if not dataset or not os.path.isfile(dataset):
            raise RuntimeError("量化需要 --dataset 指向图片路径列表 txt")
        ret = rknn.build(do_quantization=True, dataset=dataset)
    else:
        ret = rknn.build(do_quantization=False)
    if ret != 0:
        raise RuntimeError(f"build failed: {ret}")

    os.makedirs(os.path.dirname(rknn_path) or ".", exist_ok=True)
    ret = rknn.export_rknn(rknn_path)
    if ret != 0:
        raise RuntimeError(f"export_rknn failed: {ret}")

    rknn.release()
    print(f"[OK] {rknn_path}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--detect", required=True, help="YuNet onnx")
    ap.add_argument("--recog", required=True, help="SFace onnx")
    ap.add_argument("--out", default="models/rknn", help="输出目录")
    ap.add_argument("--quant", action="store_true", help="INT8 量化（需 --dataset）")
    ap.add_argument("--dataset", default=None, help="量化用图片列表 txt，每行一张图路径")
    args = ap.parse_args()

    try:
        from rknn.api import RKNN  # noqa: F401
    except ImportError:
        print("未安装 rknn-toolkit2。请在 Ubuntu x86 上：")
        print("  pip install rknn-toolkit2")
        print("或下载官方 whl: https://github.com/airockchip/rknn-toolkit2")
        sys.exit(1)

    out = args.out
    os.makedirs(out, exist_ok=True)

    detect_out = os.path.join(out, "face_detection_yunet_2022mar.rknn")
    recog_out = os.path.join(out, "face_recognition_sface_2021dec.rknn")

    # 检测：320x240；识别：112x112
    convert_one(
        args.detect,
        detect_out,
        input_size=(320, 240),
        mean_values=[[0, 0, 0]],
        std_values=[[255, 255, 255]],
        do_quantization=args.quant,
        dataset=args.dataset,
    )
    convert_one(
        args.recog,
        recog_out,
        input_size=(112, 112),
        mean_values=[[0, 0, 0]],
        std_values=[[255, 255, 255]],
        do_quantization=args.quant,
        dataset=args.dataset,
    )

    print("\n全部完成。下一步才是改 FaceRecognizer 用 librknnrt 加载这两个 .rknn。")
    print("当前程序仍读 .onnx，仅转换不会自动加速。")


if __name__ == "__main__":
    main()

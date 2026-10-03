#!/usr/bin/env bash
# =============================================================================
# 生成 MinGW 可用的 SeetaFace6 导入库（.a）
#
# 背景：SeetaFace6 官方 Windows SDK 只有 MSVC 编译的 .lib/.dll，
#       项目使用 MinGW（Qt 5.14.2 + mingw730_64），无法直接链接 MSVC .lib。
#
# 原理（x64 下 MSVC 与 MinGW-w64 调用约定一致，this 均在 RCX，参数依次
#       RDX/R8/R9，所以可以二进制级桥接）：
#   1. gendef   —— 从 DLL 导出表生成 .def（保留原始 MSVC mangled 符号名）
#   2. dlltool  —— 由 .def 生成 MinGW 导入库（跳板符号 T + __imp_ 符号 I，
#                  .idata 内按原始符号名导入，加载器按名字匹配 DLL 导出表）
#   3. objcopy  —— 把跳板正文符号（T）改名为易用的 C 别名；__imp_ 符号与
#                  .idata 里的字符串保持原始名不变，因此运行时仍按原始
#                  MSVC 符号名解析，不会出现 0xC0000139 (ENTRYPOINT_NOT_FOUND)
#
# 用法：在 Git Bash 中执行  bash build_import_libs.sh
# 需要：D:/Qt/Qt5.14.2/Tools/mingw730_64/bin 下的 gendef.exe/dlltool.exe/objcopy.exe
# 产物：lib/libSeetaFaceAntiSpoofingX600.a  lib/libSeetaFaceLandmarker600.a
# 注意：这是构建材料，生成的 .a 已提交在 lib/ 下，正常开发无需重跑本脚本
# =============================================================================
set -e

MINGW_BIN="/d/Qt/Qt5.14.2/Tools/mingw730_64/bin"
export PATH="$MINGW_BIN:$PATH"

HERE="$(cd "$(dirname "$0")" && pwd)"
BIN_DIR="$HERE/bin"
LIB_DIR="$HERE/lib"
DEF_DIR="$HERE"

# ---- 符号改名表：C 别名 = 原始 MSVC mangled 符号 ----
# 关键：只改正文（T）符号，__imp_ 符号保持原名
declare -A FAS_RENAME=(
  ['??0FaceAntiSpoofing@v6@seeta@@QEAA@AEBVModelSetting@2@@Z']='sf6_fas_create'
  ['??1FaceAntiSpoofing@v6@seeta@@QEAA@XZ']='sf6_fas_destroy'
  ['?Predict@FaceAntiSpoofing@v6@seeta@@QEBA?AW4Status@123@AEBUSeetaImageData@@AEBUSeetaRect@@PEBUSeetaPointF@@@Z']='sf6_fas_predict'
  ['?PredictVideo@FaceAntiSpoofing@v6@seeta@@QEBA?AW4Status@123@AEBUSeetaImageData@@AEBUSeetaRect@@PEBUSeetaPointF@@@Z']='sf6_fas_predict_video'
  ['?ResetVideo@FaceAntiSpoofing@v6@seeta@@QEAAXXZ']='sf6_fas_reset_video'
  ['?SetThreshold@FaceAntiSpoofing@v6@seeta@@QEAAXMM@Z']='sf6_fas_set_threshold'
  ['?GetPreFrameScore@FaceAntiSpoofing@v6@seeta@@QEAAXPEAM0@Z']='sf6_fas_get_pre_frame_score'
  ['?SetVideoFrameCount@FaceAntiSpoofing@v6@seeta@@QEAAXH@Z']='sf6_fas_set_video_frame_count'
)
declare -A LM_RENAME=(
  ['??0FaceLandmarker@v6@seeta@@QEAA@AEBUSeetaModelSetting@@@Z']='sf6_lm_create'
  ['??1FaceLandmarker@v6@seeta@@QEAA@XZ']='sf6_lm_destroy'
  ['?number@FaceLandmarker@v6@seeta@@QEBAHXZ']='sf6_lm_number'
  ['?mark@FaceLandmarker@v6@seeta@@QEBAXAEBUSeetaImageData@@AEBUSeetaRect@@PEAUSeetaPointF@@@Z']='sf6_lm_mark'
)

build_one() {
  local dll="$1" def="$2" out="$3"
  shift 3
  local -n RENAME_TABLE="$1"

  echo "== $dll =="
  dlltool.exe -d "$def" -l "$out.tmp" -D "$dll.dll" || return 1

  local args=()
  for sym in "${!RENAME_TABLE[@]}"; do
    args+=(--redefine-sym "$sym=${RENAME_TABLE[$sym]}")
  done
  objcopy "${args[@]}" "$out.tmp" "$out" || return 1
  rm -f "$out.tmp"
  echo "   -> $out"
}

build_one "SeetaFaceAntiSpoofingX600" "$DEF_DIR/SeetaFaceAntiSpoofingX600.def" \
          "$LIB_DIR/libSeetaFaceAntiSpoofingX600.a" FAS_RENAME
build_one "SeetaFaceLandmarker600"   "$DEF_DIR/SeetaFaceLandmarker600.def" \
          "$LIB_DIR/libSeetaFaceLandmarker600.a" LM_RENAME

echo "全部完成。lib/ 下产物："
ls -la "$LIB_DIR"

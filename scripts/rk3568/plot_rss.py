#!/usr/bin/env python3
"""把 rss_sample.sh 生成的 CSV 画成折线图，看 RSS 是否持续上涨。"""
import sys

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "dms_rss.csv"
    rows = []
    with open(path, encoding="utf-8") as f:
        header = f.readline().strip().split(",")
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split(",")
            if len(parts) < 4:
                continue
            rows.append([int(x) for x in parts[:4]])
    if not rows:
        print("没有数据:", path)
        return 1

    t0 = rows[0][0]
    xs = [r[0] - t0 for r in rows]
    capt = [r[1] / 1024.0 for r in rows]
    hmi = [r[2] / 1024.0 for r in rows]
    ai = [r[3] / 1024.0 for r in rows]

    def trend(name, ys):
        d = ys[-1] - ys[0]
        print(f"{name}: 起点={ys[0]:.1f}MB 终点={ys[-1]:.1f}MB 变化={d:+.1f}MB")
        if d > 20:
            print(f"  -> {name} 涨幅较大，建议延长采样再确认")
        elif abs(d) < 5:
            print(f"  -> {name} 基本平稳")

    print("==== RSS 对比 ====")
    trend("dms_capture", capt)
    trend("SmartCockpitDMS", hmi)
    trend("dms_ai", ai)

    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("未安装 matplotlib，只打印数字。可: pip install matplotlib")
        print("或用 Excel 打开 CSV，三列画折线图。")
        return 0

    out_png = path.rsplit(".", 1)[0] + ".png"
    plt.figure(figsize=(9, 5))
    plt.plot(xs, capt, label="dms_capture")
    plt.plot(xs, hmi, label="SmartCockpitDMS")
    plt.plot(xs, ai, label="dms_ai")
    plt.xlabel("time (s)")
    plt.ylabel("VmRSS (MB)")
    plt.title("DMS RSS (leak check)")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(out_png, dpi=120)
    print("图已保存:", out_png)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())

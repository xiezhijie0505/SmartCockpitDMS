#include "rknnengine.h"

#include <QFile>
#include <QDebug>
#include <opencv2/imgproc.hpp>
#include <cstring>
#include <vector>

#ifdef FRAS_HAVE_RKNN
#include "rknn_api.h"
#endif

RknnEngine::RknnEngine()
    : m_ready(false)
    , m_ctx(nullptr)
    , m_inputIndex(0)
    , m_inputWidth(0)
    , m_inputHeight(0)
    , m_inputChannels(3)
    , m_inputIsNchw(false)
    , m_inputIsFloat(false)
{
}

RknnEngine::~RknnEngine()
{
    release();
}

void RknnEngine::release()
{
#ifdef FRAS_HAVE_RKNN
    if (m_ctx) {
        rknn_destroy(reinterpret_cast<rknn_context>(m_ctx));
        m_ctx = nullptr;
    }
#endif
    m_ready = false;
}

bool RknnEngine::load(const QString &modelPath)
{
    release();
#ifndef FRAS_HAVE_RKNN
    Q_UNUSED(modelPath);
    qWarning() << "RKNN 未启用，无法加载" << modelPath;
    return false;
#else
    QFile f(modelPath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开 RKNN 模型" << modelPath;
        return false;
    }
    const QByteArray bytes = f.readAll();
    f.close();
    if (bytes.isEmpty()) {
        qWarning() << "RKNN 模型为空" << modelPath;
        return false;
    }

    rknn_context ctx = 0;
    int ret = rknn_init(&ctx, const_cast<char *>(bytes.data()), bytes.size(), 0, nullptr);
    if (ret != RKNN_SUCC) {
        qWarning() << "rknn_init 失败" << ret << modelPath;
        return false;
    }
    m_ctx = reinterpret_cast<void *>(ctx);

    rknn_input_output_num ioNum;
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &ioNum, sizeof(ioNum));
    if (ret != RKNN_SUCC || ioNum.n_input < 1) {
        qWarning() << "rknn_query IN_OUT_NUM 失败" << ret;
        release();
        return false;
    }

    std::vector<rknn_tensor_attr> inputAttrs(ioNum.n_input);
    for (uint32_t i = 0; i < ioNum.n_input; ++i) {
        inputAttrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &inputAttrs[i], sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC) {
            qWarning() << "rknn_query INPUT_ATTR 失败" << ret;
            release();
            return false;
        }
    }

    m_inputIndex = 0;
    const rknn_tensor_attr &in = inputAttrs[0];
    m_inputIsNchw = (in.fmt == RKNN_TENSOR_NCHW);
    m_inputIsFloat = (in.type == RKNN_TENSOR_FLOAT16 || in.type == RKNN_TENSOR_FLOAT32);
    if (m_inputIsNchw) {
        m_inputChannels = static_cast<int>(in.dims[1]);
        m_inputHeight = static_cast<int>(in.dims[2]);
        m_inputWidth = static_cast<int>(in.dims[3]);
    } else {
        m_inputHeight = static_cast<int>(in.dims[1]);
        m_inputWidth = static_cast<int>(in.dims[2]);
        m_inputChannels = static_cast<int>(in.dims[3]);
    }

    qDebug() << QStringLiteral("【启动】RKNN 模型已加载") << modelPath
             << "input" << m_inputWidth << "x" << m_inputHeight
             << "nchw" << m_inputIsNchw << "float" << m_inputIsFloat;
    m_ready = true;
    return true;
#endif
}

bool RknnEngine::infer(const cv::Mat &nhwcU8, QVector<cv::Mat> *outputs)
{
    if (!m_ready || !outputs) {
        return false;
    }
    outputs->clear();
#ifndef FRAS_HAVE_RKNN
    Q_UNUSED(nhwcU8);
    return false;
#else
    if (nhwcU8.empty() || nhwcU8.type() != CV_8UC3) {
        qWarning() << "RKNN 输入需为 CV_8UC3 NHWC";
        return false;
    }

    cv::Mat resized;
    if (nhwcU8.cols != m_inputWidth || nhwcU8.rows != m_inputHeight) {
        cv::resize(nhwcU8, resized, cv::Size(m_inputWidth, m_inputHeight));
    } else {
        resized = nhwcU8;
    }
    // 转换脚本按 OpenCV 习惯用 BGR + mean0/std255，保持 BGR 送入
    rknn_context ctx = reinterpret_cast<rknn_context>(m_ctx);
    rknn_input input;
    memset(&input, 0, sizeof(input));
    input.index = m_inputIndex;
    input.type = RKNN_TENSOR_UINT8;
    input.fmt = RKNN_TENSOR_NHWC;
    input.size = static_cast<uint32_t>(resized.total() * resized.elemSize());
    input.buf = resized.data;
    input.pass_through = 0;

    int ret = rknn_inputs_set(ctx, 1, &input);
    if (ret != RKNN_SUCC) {
        qWarning() << "rknn_inputs_set 失败" << ret;
        return false;
    }
    ret = rknn_run(ctx, nullptr);
    if (ret != RKNN_SUCC) {
        qWarning() << "rknn_run 失败" << ret;
        return false;
    }

    rknn_input_output_num ioNum;
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &ioNum, sizeof(ioNum));
    if (ret != RKNN_SUCC) {
        return false;
    }

    std::vector<rknn_output> outs(ioNum.n_output);
    memset(outs.data(), 0, outs.size() * sizeof(rknn_output));
    for (uint32_t i = 0; i < ioNum.n_output; ++i) {
        outs[i].want_float = 1;
        outs[i].is_prealloc = 0;
    }
    ret = rknn_outputs_get(ctx, ioNum.n_output, outs.data(), nullptr);
    if (ret != RKNN_SUCC) {
        qWarning() << "rknn_outputs_get 失败" << ret;
        return false;
    }

    for (uint32_t i = 0; i < ioNum.n_output; ++i) {
        rknn_tensor_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.index = i;
        rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &attr, sizeof(attr));
        int elems = 1;
        for (uint32_t d = 0; d < attr.n_dims; ++d) {
            elems *= static_cast<int>(attr.dims[d]);
        }
        cv::Mat m(1, elems, CV_32F);
        memcpy(m.data, outs[i].buf, static_cast<size_t>(elems) * sizeof(float));
        outputs->push_back(m.clone());
    }

    rknn_outputs_release(ctx, ioNum.n_output, outs.data());
    return !outputs->isEmpty();
#endif
}

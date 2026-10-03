#ifndef DATAPATHS_H
#define DATAPATHS_H

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>

// 按「可执行文件旁 / 上一级 / 项目根常见布局」查找资源，兼容 Windows 本机与 RK3568 部署。
inline QString findDataFile(const QStringList &relativeCandidates)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList roots;
    roots << appDir
          << appDir + "/.."
          << appDir + "/../.."
          << appDir + "/../../..";

    for (const QString &root : roots) {
        for (const QString &rel : relativeCandidates) {
            const QString path = QDir(root).absoluteFilePath(rel);
            if (QFileInfo::exists(path)) {
                return QFileInfo(path).absoluteFilePath();
            }
        }
    }

    // 找不到时返回首选路径，便于错误提示
    if (!relativeCandidates.isEmpty()) {
        return QDir(appDir).absoluteFilePath(relativeCandidates.first());
    }
    return QString();
}

#endif // DATAPATHS_H

#include "mainwindow.h"
#include <QMessageBox>
#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QFont>
#include <QFontInfo>
#include <QFontDatabase>
#include <QDir>
#include "models/databasemanager.h"
#include <QFile>
#include <QMetaType>
#include <opencv2/core/mat.hpp>
#ifdef Q_OS_LINUX
#include <QTextCodec>
#include <locale.h>
#include <time.h>
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_LINUX
    // Buildroot 通常没有 C.UTF-8，强设会刷 setlocale 警告；中文靠下面 QTextCodec
    if (!setlocale(LC_ALL, "C.UTF-8")
        && !setlocale(LC_ALL, "en_US.UTF-8")
        && !setlocale(LC_ALL, "C")) {
        setlocale(LC_ALL, "POSIX");
    }
    // 不要 export LC_ALL=C.UTF-8，否则 shell/子进程也会报警
    qunsetenv("LC_ALL");
    if (qgetenv("LANG").isEmpty() || qgetenv("LANG").contains("UTF-8")) {
        qputenv("LANG", "C");
    }
    // 告警/日志时间按本地时区；板端常缺 zoneinfo，用 POSIX 东八区
    if (qgetenv("TZ").isEmpty()) {
        qputenv("TZ", "CST-8");
        tzset();
    }
#endif

    QApplication a(argc, argv);
    qRegisterMetaType<cv::Mat>("cv::Mat");
    a.setApplicationName(QStringLiteral("SmartCockpitDMS"));
    a.setApplicationDisplayName(QStringLiteral("智能座舱 · 驾驶员监测"));

#ifdef Q_OS_LINUX
    if (QTextCodec *utf8 = QTextCodec::codecForName("UTF-8")) {
        QTextCodec::setCodecForLocale(utf8);
    }
#endif

#ifdef Q_OS_LINUX
    // linuxfb 默认找应用旁 lib/fonts；把系统字体挂进去并设可渲染的字体族
    {
        const QString appFontDir = QCoreApplication::applicationDirPath() + "/lib/fonts";
        if (QDir(appFontDir).exists()) {
            qputenv("QT_QPA_FONTDIR", appFontDir.toUtf8());
            const QStringList files = QDir(appFontDir).entryList(
                QStringList() << "*.ttf" << "*.otf" << "*.ttc", QDir::Files);
            for (const QString &fn : files) {
                QFontDatabase::addApplicationFont(appFontDir + "/" + fn);
            }
        }
        QFont uiFont("Noto Sans CJK SC");
        if (!QFontInfo(uiFont).exactMatch()) {
            uiFont = QFont("Source Han Sans CN");
        }
        if (!QFontInfo(uiFont).exactMatch()) {
            uiFont = QFont("DejaVu Sans");
        }
        uiFont.setPixelSize(16);
        a.setFont(uiFont);
    }
#endif

    // 初始化数据库（建表等操作自动完成）
    if (!DatabaseManager::getInstance().init()) {
        QMessageBox::critical(nullptr, "错误", "数据库初始化失败，请检查程序运行权限。");
        return -1;  // 初始化失败则退出程序
    }
    // 加载样式表（DJI 主题，资源文件 style.qss 由 resources.qrc 注册）
    QFile styleFile(":/style.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        QString style = QLatin1String(styleFile.readAll());
#ifdef Q_OS_LINUX
        // Windows 字体名在板子上不存在，会导致按钮有色无字
        style.replace(QStringLiteral("Microsoft YaHei"), QStringLiteral("Noto Sans CJK SC"));
        style.replace(QStringLiteral("PingFang SC"), QStringLiteral("Source Han Sans CN"));
        style.replace(QStringLiteral("Helvetica Neue"), QStringLiteral("DejaVu Sans"));
#endif
        a.setStyleSheet(style);
        styleFile.close();
    } else {
        // 资源加载失败时退回文件方式（便于调试）
        QFile styleFileDisk("style.qss");
        if (styleFileDisk.open(QFile::ReadOnly)) {
            QString style = QLatin1String(styleFileDisk.readAll());
#ifdef Q_OS_LINUX
            style.replace(QStringLiteral("Microsoft YaHei"), QStringLiteral("Noto Sans CJK SC"));
            style.replace(QStringLiteral("PingFang SC"), QStringLiteral("Source Han Sans CN"));
            style.replace(QStringLiteral("Helvetica Neue"), QStringLiteral("DejaVu Sans"));
#endif
            a.setStyleSheet(style);
            styleFileDisk.close();
        }
    }

    MainWindow w;
#ifdef Q_OS_LINUX
    // 嵌入式 linuxfb：按屏幕几何铺满，避免只露出一小块控件
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        w.setGeometry(screen->geometry());
    }
    w.setWindowState(Qt::WindowFullScreen);
    w.showFullScreen();
#else
    w.show();
#endif
    return a.exec();
}

#include "databasemanager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSqlQuery>

#include "utils/datapaths.h"

DatabaseManager::DatabaseManager():isOpen(false)
{

}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::createTables()
{
    // 1. 人员信息表
    QString sql_personnel = R"(
        CREATE TABLE IF NOT EXISTS personnel (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            gender TEXT,
            department TEXT,--部门  注意SQLite不支持c++注释 --才表示注
            position TEXT,--职位
            phone TEXT,
            face_feature BLOB UNIQUE,
            status TEXT NOT NULL DEFAULT '在职',
            remarks TEXT
        );
    )";
    if (!executeQuery(sql_personnel)) {
            return false;
        }
    // 系统设置表（键值对）
    QString sql_settings = R"(
        CREATE TABLE IF NOT EXISTS system_settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )";

    if (!executeQuery(sql_settings)) {
        return false;
    }

    qInfo() << QStringLiteral("【启动】数据库就绪");
    return true;
}

DatabaseManager &DatabaseManager::getInstance()
{
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::init()
{
    //已经打开
    if(isOpen||db.isOpen())
    {
        return true;
    }

    // 检查是否已存在默认连接，如果存在则移除并重新创建
    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        QSqlDatabase::removeDatabase("qt_sql_default_connection");
    }//这是因为 默认连接名是一样的你所以如果这个程序之前就有个一样的连接 虽然Qt会自动移除之前的 但是自己移除更好点

    //设置数据库驱动类型
    db=QSqlDatabase::addDatabase("QSQLITE");
    // 数据库路径：优先可执行文件旁 data/，兼容本机开发与板端部署
    QString dbPath = findDataFile({"data/attendance.db"});
    QDir().mkpath(QFileInfo(dbPath).absolutePath());

    //告诉对象db 数据库路径在哪里 不会打开数据库
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qWarning() << QStringLiteral("【启动】数据库打开失败") << dbPath << db.lastError().text();
        isOpen = false;
        return false;
    }

    isOpen = true;

    // 创建表
    if (!createTables()) {
        qDebug() << "建表失败";
        return false;
    }

    return true;
}

bool DatabaseManager::executeQuery(const QString &sql)
{
    if (!isOpen || !db.isOpen()) {
           qDebug() << "数据库未打开，无法执行 SQL";
           return false;
       }

       QSqlQuery query(db);
       if (!query.exec(sql)) {
           qDebug() << "SQL 执行失败：" << query.lastError().text();
           qDebug() << "失败的 SQL 语句：" << sql;
           return false;
       }
       return true;
}

void DatabaseManager::close()
{
    if (db.isOpen()) {
        db.close();
        isOpen = false;
        qDebug() << "数据库已关闭";
    }
}

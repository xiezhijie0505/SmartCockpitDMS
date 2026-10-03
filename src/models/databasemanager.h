#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

class DatabaseManager//数据库管家 负责数据库打开 关闭 建表 并未其他模块提供执行SQL语句的能力 单例模式
{
public:
    // 获取单例实例（全局唯一）
    static DatabaseManager& getInstance();

    // 初始化：打开数据库并建表
    bool init();

    // 获取数据库连接对象（供 Model 层使用）
    QSqlDatabase getDatabase() const { return db; }

    // 执行无返回值的 SQL（增、删、改）
    bool executeQuery(const QString &sql);

    // 关闭数据库
    void close();
private:
    // 构造函数私有化（单例模式）
    DatabaseManager();
    ~DatabaseManager();

    // 创建所有需要的表
    bool createTables();

    QSqlDatabase db;  // 数据库连接对象
    bool isOpen;      // 是否打开成功
};

#endif // DATABASEMANAGER_H

#include "driverarchivecontroller.h"
#include "models/drivermodel.h"
#include <QDebug>

DriverArchiveController::DriverArchiveController(QObject *parent)
    : QObject(parent)
    , m_model(nullptr)
     , m_faceRecognizer(nullptr)   // ← 新增初始化
{
}

void DriverArchiveController::setModel(DriverModel *model)
{
    m_model = model;
}

// ==================== 添加人员 ====================
bool DriverArchiveController::addPerson(const QVariantMap &personData, QString &errorMsg)
{
    if (!m_model) {
        errorMsg = "模型未初始化";
        return false;
    }

    // 1. 数据校验
    if (!validatePersonData(personData, errorMsg)) {
        return false;
    }

    // 2. 检查姓名是否重复
    QString name = personData["name"].toString();
    if (isNameExists(name)) {
        errorMsg = QString("姓名 '%1' 已存在，请勿重复添加").arg(name);
        return false;
    }

    // 3. 构造 DriverInfo 结构体
    DriverInfo info;
    info.name = name;
    info.gender = personData["gender"].toString();
    info.department = personData["department"].toString();
    info.position = personData["position"].toString();
    info.phone = personData["phone"].toString();
    info.status = personData["status"].toString();
    info.remarks = personData["remarks"].toString();
    info.faceFeature = personData["face_feature"].toByteArray();

    // 4. 调用 Model 层添加
    bool ok = m_model->addPerson(info);
    if (ok) {
        emit dataChanged();
        emit personAdded(0, info.name); // ID 由数据库自动生成，此处暂时传 0
        qDebug() << "人员添加成功：" << info.name;
    } else {
        errorMsg = "添加人员失败，请检查数据库连接";
    }
    return ok;
}

// ==================== 删除人员 ====================
bool DriverArchiveController::deletePerson(int id, QString &errorMsg)
{
    if (!m_model) {
        errorMsg = "模型未初始化";
        return false;
    }

    if (id <= 0) {
        errorMsg = "无效的人员 ID";
        return false;
    }

    bool ok = m_model->deletePerson(id);
    if (ok) {
        emit dataChanged();
        emit personDeleted(id);
        qDebug() << "人员删除成功，ID：" << id;
    } else {
        errorMsg = "删除人员失败，请检查该 ID 是否存在";
    }
    return ok;
}

// ==================== 更新人员 ====================
bool DriverArchiveController::updatePerson(int id, const QVariantMap &fields, QString &errorMsg)
{
    if (!m_model) {
        errorMsg = "模型未初始化";
        return false;
    }

    if (id <= 0) {
        errorMsg = "无效的人员 ID";
        return false;
    }

    if (fields.isEmpty()) {
        errorMsg = "没有要更新的字段";
        return false;
    }

    // 如果更新了姓名，检查是否与其他人重复
    if (fields.contains("name")) {
        QString newName = fields["name"].toString();
        if (isNameExists(newName, id)) {
            errorMsg = QString("姓名 '%1' 已被其他员工使用").arg(newName);
            return false;
        }
    }

    bool ok = m_model->updatePerson(id, fields);
    if (ok) {
        emit dataChanged();
        emit personUpdated(id);
        qDebug() << "人员更新成功，ID：" << id;
    } else {
        errorMsg = "更新人员失败，请检查 ID 是否有效";
    }
    return ok;
}

// ==================== 更新人脸特征 ====================
bool DriverArchiveController::updateFaceFeature(int id, const QByteArray &faceFeature, QString &errorMsg)
{
    if (id <= 0) {
        errorMsg = "无效的人员 ID";
        return false;
    }
    if (faceFeature.isEmpty()) {
        errorMsg = "人脸特征为空";
        return false;
    }

    QVariantMap fields;
    fields["face_feature"] = faceFeature;
    return updatePerson(id, fields, errorMsg);
}

// ==================== 查询 ====================
QList<QVariantMap> DriverArchiveController::getAllPersons()
{
    if (!m_model) return QList<QVariantMap>();
    return m_model->getAllPersons();
}

QVariantMap DriverArchiveController::getPersonById(int id)
{
    QVariantMap result;
    if (!m_model || id <= 0) return result;

    DriverInfo info = m_model->getPersonById(id);
    if (info.id != -1) {
        result = info.toMap();
        // 注意：toMap() 不包含 face_feature，如需特征请单独获取
    }
    return result;
}

QList<QVariantMap> DriverArchiveController::searchPersonsByName(const QString &keyword)
{
    if (!m_model) return QList<QVariantMap>();
    if (keyword.trimmed().isEmpty()) {
        return getAllPersons();
    }
    return m_model->findPersonsByName(keyword);
}

QList<QVariantMap> DriverArchiveController::getPersonsByDepartment(const QString &department)
{
    if (!m_model) return QList<QVariantMap>();
    if (department.trimmed().isEmpty()) {
        return getAllPersons();
    }
    return m_model->findPersonsByDepartment(department);
}

int DriverArchiveController::getTotalCount()
{
    if (!m_model) return 0;
    return m_model->getTotalCount();
}

// ==================== 工具方法 ====================
bool DriverArchiveController::isNameExists(const QString &name, int excludeId)
{
    if (!m_model || name.trimmed().isEmpty()) return false;

    // 获取所有人员并比对姓名（简单实现，数据量大时可改用 SQL 查询）
    auto list = m_model->getAllPersons();
    for (const auto &item : list) {
        int id = item["id"].toInt();
        if (id == excludeId) continue; // 排除自己
        if (item["name"].toString() == name) {
            return true;
        }
    }
    return false;
}

QList<QVariantMap> DriverArchiveController::getAllFeaturesForMatch()
{
    if (!m_model) return QList<QVariantMap>();

    // 直接使用 DriverModel 的方法
    auto featureList = m_model->getAllFeaturesForMatch();
    QList<QVariantMap> result;
    for (const auto &item : featureList) {
        QVariantMap map;
        map["id"] = item.id;
        map["name"] = item.name;
        map["face_feature"] = item.faceFeature;
        result.append(map);
    }
    return result;
}

// ==================== 私有辅助 ====================
bool DriverArchiveController::validatePersonData(const QVariantMap &data, QString &errorMsg)
{
    // 姓名必填
    QString name = data["name"].toString().trimmed();
    if (name.isEmpty()) {
        errorMsg = "姓名不能为空";
        return false;
    }

    // 手机号格式校验（简单示例，可根据需求加强）
    QString phone = data["phone"].toString().trimmed();
    if (!phone.isEmpty() && phone.length() != 11) {
        errorMsg = "手机号格式不正确（应为11位）";
        return false;
    }

    return true;
}


DriverArchiveController::ImportResult DriverArchiveController::batchImportPersons(
    const QString &csvFilePath,
    const QString &photoDirPath)
{
    ImportResult result;
    result.total = 0;
    result.success = 0;
    result.failed = 0;

    // 1. 检查模型是否初始化
    if (!m_model) {
        result.errors << "模型未初始化";
        return result;
    }

    // 2. 读取 CSV 文件
    QFile csvFile(csvFilePath);
    if (!csvFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errors << "无法打开 CSV 文件：" + csvFilePath;
        return result;
    }

    QTextStream in(&csvFile);
    in.setCodec("UTF-8");

    // 3. 读取表头，确定列索引
    QString headerLine = in.readLine();
    if (headerLine.isEmpty()) {
        result.errors << "CSV 文件为空";
        return result;
    }

    QStringList headers = headerLine.split(',', Qt::SkipEmptyParts);
    // 去除表头中的 BOM 和空格
    for (int i = 0; i < headers.size(); i++) {
        headers[i] = headers[i].trimmed();
    }

    // 查找各列索引
    int nameIdx = headers.indexOf("姓名");
    int genderIdx = headers.indexOf("性别");
    int deptIdx = headers.indexOf("部门");
    int posIdx = headers.indexOf("职位");
    int phoneIdx = headers.indexOf("手机号");
    int statusIdx = headers.indexOf("状态");
    int remarkIdx = headers.indexOf("备注");
    int photoIdx = headers.indexOf("照片文件名");

    if (nameIdx == -1) {
        result.errors << "CSV 缺少必填列：姓名";
        return result;
    }
    if (photoIdx == -1) {
        result.errors << "CSV 缺少必填列：照片文件名";
        return result;
    }

    // 4. 逐行读取数据
    int lineNum = 1;
    while (!in.atEnd()) {
        QString line = in.readLine();
        lineNum++;
        if (line.trimmed().isEmpty()) continue;

        QStringList fields = line.split(',', Qt::SkipEmptyParts);
        // 保证字段数量与表头一致
        while (fields.size() < headers.size()) {
            fields.append("");
        }

        // 提取字段
        QString name = fields[nameIdx].trimmed();
        QString photoFileName = fields[photoIdx].trimmed();

        if (name.isEmpty()) {
            result.failed++;
            result.errors << QString("第 %1 行：姓名为空").arg(lineNum);
            continue;
        }
        if (photoFileName.isEmpty()) {
            result.failed++;
            result.errors << QString("第 %1 行：照片文件名为空").arg(lineNum);
            continue;
        }

        // 5. 查找对应的照片文件
        QString photoPath = photoDirPath + "/" + photoFileName;
        if (!QFile::exists(photoPath)) {
            result.failed++;
            result.errors << QString("第 %1 行：照片文件不存在 %2").arg(lineNum).arg(photoFileName);
            continue;
        }

        // 6. 从照片提取人脸特征
        cv::Mat img = cv::imread(photoPath.toStdString());
        if (img.empty()) {
            result.failed++;
            result.errors << QString("第 %1 行：无法读取照片 %2").arg(lineNum).arg(photoFileName);
            continue;
        }

        // 注意：这里需要传入 FaceRecognizer 实例
        // 方案一：在 DriverArchiveController 中添加 setFaceRecognizer
        // 方案二：通过依赖注入传入
        // 假设已有 m_faceRecognizer 成员
        cv::Mat feature = m_faceRecognizer->extractFeature(img, false);
        if (feature.empty()) {
            result.failed++;
            result.errors << QString("第 %1 行：照片中未检测到人脸 %2").arg(lineNum).arg(photoFileName);
            continue;
        }

        // 7. 构造 DriverInfo
        QVariantMap personData;
        personData["name"] = name;
        personData["gender"] = (genderIdx != -1) ? fields[genderIdx].trimmed() : "男";
        personData["department"] = (deptIdx != -1) ? fields[deptIdx].trimmed() : "";
        personData["position"] = (posIdx != -1) ? fields[posIdx].trimmed() : "";
        personData["phone"] = (phoneIdx != -1) ? fields[phoneIdx].trimmed() : "";
        personData["status"] = (statusIdx != -1) ? fields[statusIdx].trimmed() : "在职";
        personData["remarks"] = (remarkIdx != -1) ? fields[remarkIdx].trimmed() : "";
        personData["face_feature"] = FaceRecognizer::matToByteArray(feature);

        // 8. 调用现有的 addPerson 接口
        QString errorMsg;
        if (addPerson(personData, errorMsg)) {
            result.success++;
        } else {
            result.failed++;
            result.errors << QString("第 %1 行：%2").arg(lineNum).arg(errorMsg);
        }

        result.total++;
    }

    csvFile.close();

    qDebug() << "【批量导入】完成，总数:" << result.total
             << "成功:" << result.success
             << "失败:" << result.failed;
    return result;
}
void DriverArchiveController::setFaceRecognizer(FaceRecognizer *faceRecognizer)
{
    m_faceRecognizer = faceRecognizer;
}

#pragma once
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>

namespace ChineseHotkeyConfig {
constexpr int defaultKey = Qt::Key_F3;
// Single keys supported by both Qt and Mari's native viewport adapter.
// Esc cancels recording; modifiers, punctuation and Shift+~ are not bindable.
inline bool valid(int key) {
    if ((key >= Qt::Key_F1 && key <= Qt::Key_F24) ||
        (key >= Qt::Key_A && key <= Qt::Key_Z) ||
        (key >= Qt::Key_0 && key <= Qt::Key_9)) return true;
    switch (key) {
    case Qt::Key_Space: case Qt::Key_Tab: case Qt::Key_Return:
    case Qt::Key_Backspace: case Qt::Key_Insert: case Qt::Key_Delete:
    case Qt::Key_Home: case Qt::Key_End: case Qt::Key_PageUp: case Qt::Key_PageDown:
    case Qt::Key_Left: case Qt::Key_Up: case Qt::Key_Right: case Qt::Key_Down:
        return true;
    default: return false;
    }
}
inline QString path(const QString& product) {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return base.isEmpty() ? QString() : base + QLatin1Char('/') + product + QStringLiteral("/hotkeys.json");
}
inline int load(const QString& filePath, QString* error = nullptr) {
    if (error) error->clear();
    if (filePath.isEmpty()) {
        if (error) *error = QStringLiteral("无法定位快捷键配置目录，使用 F3。");
        return defaultKey;
    }
    QFile file(filePath);
    if (!file.exists()) return defaultKey;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return defaultKey;
    }
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(file.read(4097), &parse);
    const auto object = doc.object();
    const auto value = object.value(QStringLiteral("toggleQtKey"));
    const int key = value.toInt(-1);
    if (file.size() > 4096 || parse.error != QJsonParseError::NoError || !doc.isObject() ||
        object.value(QStringLiteral("version")).toDouble(-1) != 1 ||
        !value.isDouble() || value.toDouble() != key || !valid(key)) {
        if (error) *error = QStringLiteral("快捷键配置无效，使用 F3。");
        return defaultKey;
    }
    return key;
}
inline bool save(const QString& filePath, int key, QString* error = nullptr) {
    if (error) error->clear();
    if (filePath.isEmpty() || !valid(key)) {
        if (error) *error = QStringLiteral("无效的配置路径或快捷键。");
        return false;
    }
    if (!QDir().mkpath(QFileInfo(filePath).absolutePath())) {
        if (error) *error = QStringLiteral("无法创建快捷键配置目录。");
        return false;
    }
    const QJsonObject object{{QStringLiteral("version"), 1}, {QStringLiteral("toggleQtKey"), key}};
    const QByteArray data = QJsonDocument(object).toJson();
    QSaveFile file(filePath);
    // Default QSaveFile behaviour: no non-atomic direct-write fallback.
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}
}

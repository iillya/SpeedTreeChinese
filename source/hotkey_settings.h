#pragma once
#include "hotkey_config.h"
#include <QtCore/QSet>
#include <QtGui/QKeyEvent>
#include <QtGui/QKeySequence>
#include <QtGui/QFont>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>

// Plugin-owned UI; never changes host shortcut settings or scene data.
class ToggleHotkeyDialog final : public QDialog {
    int& currentKey_;
    bool& dialogOpen_;
    const QString configPath_;
    int pendingKey_;
    bool listening_ = false;
    QSet<int> heldKeys_;
    QPushButton* capture_;
    QPushButton* ok_;
    QLabel* error_;
    bool saveKey(int key) {
        QString error;
        if (!ChineseHotkeyConfig::save(configPath_, key, &error)) {
            error_->setText(QStringLiteral("保存失败：") + error);
            return false;
        }
        currentKey_ = key;
        return true;
    }
    void stopListen() {
        listening_ = false;
        ok_->setEnabled(true);
        capture_->setText(QStringLiteral("当前：") + QKeySequence(pendingKey_).toString(QKeySequence::NativeText));
    }
public:
    ToggleHotkeyDialog(QWidget* parent, int& currentKey, bool& dialogOpen,
                       const QString& configPath)
        : QDialog(parent), currentKey_(currentKey), dialogOpen_(dialogOpen),
          configPath_(configPath), pendingKey_(currentKey) {
        setWindowTitle(QStringLiteral("中英切换快捷键"));
        setObjectName(QStringLiteral("qt_chinese_hotkey_settings"));
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowModality(Qt::WindowModal);
        setFixedWidth(360);
        setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
        // Scope every rule to our dialog. Never style the host application.
        setStyleSheet(QStringLiteral(
            "QDialog#qt_chinese_hotkey_settings { background: #303030; }"
            "QDialog#qt_chinese_hotkey_settings QLabel { color: #bdbdbd; background: transparent; font-size: 12px; }"
            "QDialog#qt_chinese_hotkey_settings QLabel#hotkeyInstruction { color: #e6e6e6; font-size: 13px; }"
            "QDialog#qt_chinese_hotkey_settings QLabel#hotkeyError { color: #ffb4a8; }"
            "QDialog#qt_chinese_hotkey_settings QPushButton { color: #e6e6e6; background: #3f3f3f; border: 1px solid #565656; border-radius: 4px; padding: 5px 12px; min-height: 18px; }"
            "QDialog#qt_chinese_hotkey_settings QPushButton:hover { background: #4c4c4c; }"
            "QDialog#qt_chinese_hotkey_settings QPushButton:pressed { background: #333333; }"
            "QDialog#qt_chinese_hotkey_settings QPushButton:focus { border-color: #66aaff; }"
            "QDialog#qt_chinese_hotkey_settings QPushButton#hotkeyCapture { font-size: 13px; min-height: 18px; }"
            "QDialog#qt_chinese_hotkey_settings QPushButton#hotkeyAccept { border-color: #66aaff; }"
            "QDialog#qt_chinese_hotkey_settings QPushButton#hotkeyAccept:hover { background: #4c4c4c; }"
            "QDialog#qt_chinese_hotkey_settings QPushButton#hotkeyAccept:pressed { background: #333333; }"
            "QDialog#qt_chinese_hotkey_settings QPushButton:disabled { background: #353535; color: #888888; border-color: #565656; }"));
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(10);
        auto* instruction = new QLabel(QStringLiteral("点击下方按钮，然后按下新的快捷键。"), this);
        instruction->setObjectName(QStringLiteral("hotkeyInstruction"));
        layout->addWidget(instruction);
        capture_ = new QPushButton(this);
        capture_->setObjectName(QStringLiteral("hotkeyCapture"));
        layout->addWidget(capture_);
        layout->addWidget(new QLabel(
            QStringLiteral("按 Esc 键取消录入。单击“确定”保存，下次启动时继续使用。"), this));
        layout->addWidget(new QLabel(
            QStringLiteral("按 Shift + ~ 可嗅探未翻译的 UI 文本。"), this));
        error_ = new QLabel(this);
        error_->setObjectName(QStringLiteral("hotkeyError"));
        error_->setWordWrap(true);
        layout->addWidget(error_);
        auto* row = new QHBoxLayout;
        layout->addLayout(row);
        ok_ = new QPushButton(QStringLiteral("确定"), this);
        auto* reset = new QPushButton(QStringLiteral("重置"), this);
        auto* cancel = new QPushButton(QStringLiteral("取消"), this);
        ok_->setObjectName(QStringLiteral("hotkeyAccept"));
        reset->setObjectName(QStringLiteral("hotkeyReset"));
        cancel->setObjectName(QStringLiteral("hotkeyCancel"));
        row->addWidget(reset);
        row->addStretch();
        row->addWidget(cancel);
        row->addWidget(ok_);
        ok_->setDefault(true);
        reset->setAutoDefault(false);
        cancel->setAutoDefault(false);
        capture_->setAutoDefault(false);
        row->setSpacing(8);
        setFocusProxy(ok_);
        QObject::connect(capture_, &QPushButton::clicked, this, [this] {
            listening_ = true;
            error_->clear();
            ok_->setEnabled(false);
            capture_->setText(QStringLiteral("等待按键…"));
            capture_->setFocus();
        });
        QObject::connect(ok_, &QPushButton::clicked, this, [this] { accept(); });
        QObject::connect(cancel, &QPushButton::clicked, this, [this] { reject(); });
        QObject::connect(reset, &QPushButton::clicked, this, [this] {
            const int defaultKey = ChineseHotkeyConfig::defaultKey;
            if (!saveKey(defaultKey)) return;
            pendingKey_ = defaultKey;
            error_->clear();
            stopListen();
        });
        stopListen();
        dialogOpen_ = true;
        QApplication::instance()->installEventFilter(this);
    }
    ~ToggleHotkeyDialog() override {
        QApplication::instance()->removeEventFilter(this);
        dialogOpen_ = false;
    }
protected:
    void accept() override {
        if (listening_) return;
        if (!saveKey(pendingKey_)) return;
        QDialog::accept();
    }
    void reject() override { stopListen(); QDialog::reject(); }
    bool eventFilter(QObject* object, QEvent* event) override {
        if (object == this && event->type() == QEvent::WindowDeactivate) {
            heldKeys_.clear();
            stopListen();
        }
        auto* widget = qobject_cast<QWidget*>(object);
        if (!isVisible() || !widget || widget->window() != this) return false;
        if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease &&
            event->type() != QEvent::ShortcutOverride) return false;
        auto* key = static_cast<QKeyEvent*>(event);
        if (!listening_ && !heldKeys_.contains(key->key())) return false;
        if (event->type() == QEvent::ShortcutOverride) { event->accept(); return true; }
        if (event->type() == QEvent::KeyRelease) {
            if (!key->isAutoRepeat()) heldKeys_.remove(key->key());
            return true;
        }
        if (key->isAutoRepeat()) return true;
        heldKeys_.insert(key->key());
        if (!listening_) return true;
        if (key->key() == Qt::Key_Escape) { stopListen(); return true; }
        if (key->modifiers() != Qt::NoModifier || !ChineseHotkeyConfig::valid(key->key())) {
            error_->setText(QStringLiteral("请按单个字母、数字、功能键或导航键，不使用组合键。"));
            return true;
        }
        pendingKey_ = key->key();
        error_->clear();
        stopListen();
        return true;
    }
};

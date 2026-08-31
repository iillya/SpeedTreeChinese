#include "../hotkey_settings.h"
#include <QtCore/QTemporaryDir>
#include <QtCore/QProcess>
#include <QtCore/QPointer>
#include <QtGui/QFontDatabase>
// Qt 6.6's qtestmouse.h leaves a switch-local enum uninitialized on an
// unreachable branch. Keep /WX for our code while isolating this SDK warning.
#pragma warning(push)
#pragma warning(disable:4701)
#include <QtTest/QTest>
#pragma warning(pop)
#include <cstdio>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(false)
static void flushDeletes() {
    QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}
static QPushButton* button(ToggleHotkeyDialog* w, const char* name) {
    return w->findChild<QPushButton*>(QString::fromLatin1(name));
}
int main(int argc, char** argv) {
    QApplication app(argc,argv);
    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == "--preview")
        QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
    namespace Config = ChineseHotkeyConfig;
    if (argc == 4 && QString::fromLocal8Bit(argv[1]) == "--check") {
        QString error;
        const int key = Config::load(QString::fromLocal8Bit(argv[2]),&error);
        return error.isEmpty() && key == QString::fromLocal8Bit(argv[3]).toInt() ? 0 : 1;
    }
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/用户配置/hotkeys.json");
    QString error;
    CHECK(Config::load(path,&error) == Qt::Key_F3 && error.isEmpty());
    CHECK(Config::path("SpeedTreeChinese") != Config::path("MariChinese"));
    int current = Qt::Key_F3;
    bool open = false;
    auto make = [&](const QString& p) {
        auto* w = new ToggleHotkeyDialog(nullptr,current,open,p);
        w->show(); w->activateWindow(); QCoreApplication::processEvents();
        return w;
    };
    QPointer<ToggleHotkeyDialog> w(make(path));
    CHECK(w && open);
    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == "--preview") {
        QTest::qWait(100);
        CHECK(w->grab().save(QString::fromLocal8Bit(argv[2])));
        return 0;
    }
    QTest::mouseClick(button(w,"hotkeyCapture"),Qt::LeftButton);
    QTest::keyClick(button(w,"hotkeyCapture"),Qt::Key_F6);
    CHECK(button(w,"hotkeyCapture")->text().contains("F6"));
    CHECK(current == Qt::Key_F3 && !QFile::exists(path));
    QTest::mouseClick(button(w,"hotkeyAccept"),Qt::LeftButton);
    CHECK(current == Qt::Key_F6 && Config::load(path) == Qt::Key_F6);
    flushDeletes(); CHECK(!w && !open);
    QProcess child;
    child.start(QCoreApplication::applicationFilePath(),
                QStringList() << "--check" << path << QString::number(Qt::Key_F6));
    CHECK(child.waitForFinished(10000) && child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0);
    w = make(path);
    CHECK(button(w,"hotkeyCapture")->text().contains("F6"));
    QTest::mouseClick(button(w,"hotkeyCapture"),Qt::LeftButton);
    QTest::keyClick(button(w,"hotkeyCapture"),Qt::Key_Escape);
    CHECK(w->isVisible() && button(w,"hotkeyAccept")->isEnabled());
    CHECK(button(w,"hotkeyCapture")->text().contains("F6"));
    QTest::mouseClick(button(w,"hotkeyCapture"),Qt::LeftButton);
    QTest::keyClick(button(w,"hotkeyCapture"),Qt::Key_F7,Qt::ControlModifier);
    CHECK(!button(w,"hotkeyAccept")->isEnabled());
    QTest::keyClick(button(w,"hotkeyCapture"),Qt::Key_A);
    CHECK(button(w,"hotkeyCapture")->text().contains("A"));
    QTest::mouseClick(button(w,"hotkeyCancel"),Qt::LeftButton);
    flushDeletes(); CHECK(!open && current == Qt::Key_F6 && Config::load(path) == Qt::Key_F6);
    w = make(path);
    QTest::mouseClick(button(w,"hotkeyReset"),Qt::LeftButton);
    CHECK(current == Qt::Key_F6);
    w->close(); flushDeletes();
    CHECK(Config::load(path) == Qt::Key_F6 && !open);
    w = make(path);
    QTest::mouseClick(button(w,"hotkeyReset"),Qt::LeftButton);
    QTest::mouseClick(button(w,"hotkeyAccept"),Qt::LeftButton);
    flushDeletes(); CHECK(current == Qt::Key_F3 && Config::load(path) == Qt::Key_F3 && !open);
    w = make(dir.path()); // Atomic write must fail when destination is a directory.
    QTest::mouseClick(button(w,"hotkeyCapture"),Qt::LeftButton);
    QTest::keyClick(button(w,"hotkeyCapture"),Qt::Key_F8);
    QTest::mouseClick(button(w,"hotkeyAccept"),Qt::LeftButton);
    CHECK(w->isVisible() && current == Qt::Key_F3);
    CHECK(!w->findChild<QLabel*>("hotkeyError")->text().isEmpty());
    w->close(); flushDeletes(); CHECK(!open);
    CHECK(!Config::save(path,Qt::Key_Shift));
    CHECK(Config::load(path) == Qt::Key_F3);
    for (const QByteArray data : {QByteArray("{"), QByteArray("[]"),
             QByteArray("{\"version\":1,\"toggleQtKey\":65.5}"),
             QByteArray("{\"version\":2,\"toggleQtKey\":65}"),
             QByteArray("{\"version\":1,\"toggleQtKey\":16777216}"),
             QByteArray(4097,' ')}) {
        QFile bad(path); CHECK(bad.open(QIODevice::WriteOnly)); CHECK(bad.write(data) == data.size()); bad.close();
        CHECK(Config::load(path,&error) == Qt::Key_F3 && !error.isEmpty());
    }
    CHECK(Config::save(path,Qt::Key_Z,&error) && error.isEmpty());
    CHECK(Config::load(path,&error) == Qt::Key_Z && error.isEmpty());
    CHECK(!Config::save(QString(),Qt::Key_F3));
    CHECK(Config::valid(Qt::Key_F24) && !Config::valid(Qt::Key_Escape));
    w = make(path);
    QTest::mouseClick(button(w,"hotkeyCapture"),Qt::LeftButton);
    delete w.data(); CHECK(!w && !open);
    // Recording navigation/default-button keys must not activate UI controls.
    for (int key : {Qt::Key_Return, Qt::Key_Tab, Qt::Key_Space, Qt::Key_F24}) {
        w = make(path);
        QTest::mouseClick(button(w,"hotkeyCapture"),Qt::LeftButton);
        QTest::keyClick(button(w,"hotkeyCapture"),static_cast<Qt::Key>(key));
        CHECK(w && w->isVisible() && button(w,"hotkeyAccept")->isEnabled());
        CHECK(current == Qt::Key_F3 && Config::load(path) == Qt::Key_Z);
        w->close(); flushDeletes(); CHECK(!open);
    }
    w = make(path);
    QTest::mouseClick(button(w,"hotkeyCapture"),Qt::LeftButton);
    QEvent deactivate(QEvent::WindowDeactivate);
    QApplication::sendEvent(w,&deactivate);
    CHECK(button(w,"hotkeyAccept")->isEnabled());
    w->close(); flushDeletes(); CHECK(!open);
    std::printf("PASS Qt %s: recording, commit/cancel/reset, failure, invalid config, separate-process restart\n",qVersion());
    return 0;
}

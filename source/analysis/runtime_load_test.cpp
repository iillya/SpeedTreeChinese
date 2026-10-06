// Loads the actual release DLL into an isolated Qt host; no commercial host,
// project, screenshot or user configuration is opened.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <cstdio>
#include <cstdlib>
#include <memory>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (false)

int main(int argc, char** argv) {
    QStandardPaths::setTestModeEnabled(true);
    CHECK(argc >= 3);
    const QByteArray mode = argc > 3 ? argv[3] : "late";
    const DWORD delay = argc > 4 ? DWORD(std::strtoul(argv[4], nullptr, 10)) : 200;
    const QFileInfo hook(QString::fromLocal8Bit(argv[1]));
    const std::wstring path = hook.absoluteFilePath().toStdWString();
    HMODULE module = nullptr;
    if (mode == "early" || mode == "early-shutdown" || mode == "no-app-exit") {
        module = LoadLibraryW(path.c_str());
        CHECK(module);
        Sleep(mode == "early-shutdown" ? 200 : delay);
        if (mode == "no-app-exit") {
            std::printf("PASS Qt %s: release DLL before QApplication, process exit while worker waits\n", qVersion());
            return 0;
        }
    }
    auto application = std::make_unique<QApplication>(argc, argv);
    QApplication& app = *application;
    if (mode == "early-shutdown") {
        Sleep(delay);
        application.reset();
        Sleep(250);
        std::printf("PASS Qt %s: early DLL, QApplication destruction without event-loop dispatch, delay=%lu\n", qVersion(), delay);
        return 0;
    }
    if (mode == "delayed-gui") {
        module = LoadLibraryW(path.c_str());
        CHECK(module);
        QElapsedTimer settle; settle.start();
        while (settle.elapsed() < 500) {
            QApplication::processEvents();
            Sleep(5);
        }
    }
    QFile dictionary(hook.absolutePath() + QStringLiteral("/translations/dictionary_zh.json"));
    CHECK(dictionary.open(QIODevice::ReadOnly));
    const QString source = QStringLiteral("File");
    const QString target = QJsonDocument::fromJson(dictionary.readAll()).object()
        .value(QStringLiteral("translations")).toObject().value(source).toString();
    CHECK(!target.isEmpty() && target != source);
    const QFontMetrics metrics(app.font());
    const QSize originalMetrics = metrics.size(Qt::TextSingleLine, source);
    QMainWindow window;
    window.menuBar()->addAction(source);
    window.menuBar()->addAction(QStringLiteral("Edit"));
    window.menuBar()->addAction(QStringLiteral("View"));
    window.menuBar()->addAction(QStringLiteral("Help"));
    window.show();
    if (!module) module = LoadLibraryW(path.c_str());
    CHECK(module);
    QElapsedTimer timer; timer.start();
    while (!window.findChild<QAction*>(QString::fromLocal8Bit(argv[2])) && timer.elapsed() < 5000) {
        QApplication::processEvents();
        Sleep(5);
    }
    CHECK(window.findChild<QAction*>(QString::fromLocal8Bit(argv[2])));
    QLabel label(source), reference(target);
    CHECK(label.text() == source);
    CHECK(label.sizeHint() == reference.sizeHint());
    CHECK(metrics.size(Qt::TextSingleLine, source) == originalMetrics);
    CHECK(FreeLibrary(module));
    CHECK(GetModuleHandleW(hook.fileName().toStdWString().c_str()) == module);
    QApplication::processEvents();
    CHECK(label.sizeHint() == reference.sizeHint());
    window.close();
    QApplication::processEvents();
    std::printf("PASS Qt %s release DLL (%s): GUI initialization, translated control metrics, source text/metrics, lifetime pin, window close\n", qVersion(), mode.constData());
    return 0;
}

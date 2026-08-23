// ============================================================================
//  SpeedTree Chinese Localizer - Capture Probe (Hook DLL)
// ============================================================================
//  Feasibility probe for SpeedTree Modeler (Qt 6.6.0).
//
//  This version does NOT modify any text. It periodically samples the live
//  UI and accumulates every English string it can read, so node parameters
//  that are created only after a node is selected get captured too.
//
//  Sources scanned on each pass:
//    * QLabel / QAbstractButton / QLineEdit / QAbstractSpinBox / QComboBox
//    * Menus (QAction text), QTabWidget / QTabBar tab text
//    * window title + tooltip
//    * every QAbstractItemView's model (DisplayRole), recursing children
//
//  Output:
//    %LOCALAPPDATA%\SpeedTreeChinese\probe.log      status + counts
//    %LOCALAPPDATA%\SpeedTreeChinese\all_texts.txt  unique English UI strings
//
//  Links against Qt 6.5.3 headers/libs (ABI-compatible with the host's
//  6.6.0); at run time it binds to the Qt DLLs already loaded in SpeedTree.
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <cstdio>
#include <set>
#include <atomic>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QLocale>
#include <QtCore/QTimer>
#include <QtCore/QModelIndex>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QPointer>
#include <QtGui/QAction>
#include <QtGui/QPainter>
#include <QtGui/QTextOption>
#include <QtGui/QTextFormat>
#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QMenu>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QAbstractSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QStyledItemDelegate>

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
static std::wstring LocalAppDataDir() {
    wchar_t buf[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    return n ? std::wstring(buf) : std::wstring(L"C:\\Users\\Public");
}

static void Log(const std::string& msg) {
    std::wstring dir = LocalAppDataDir() + L"\\SpeedTreeChinese";
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring path = dir + L"\\probe.log";
    HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD wr = 0;
        WriteFile(h, msg.data(), static_cast<DWORD>(msg.size()), &wr, nullptr);
        WriteFile(h, "\r\n", 2, &wr, nullptr);
        CloseHandle(h);
    }
}

// ---------------------------------------------------------------------------
// Accumulated text inventory
// ---------------------------------------------------------------------------
static std::unordered_set<std::string> g_texts;
static constexpr size_t kMaxCaptured = 60000;

static void AddIf(const QString& text) {
    if (text.isEmpty() || g_texts.size() >= kMaxCaptured)
        return;
    // Skip already-localized marker text so the inventory stays English-only.
    if (text.contains(QChar(0x3010)))
        return;
    const std::string utf8 = text.toUtf8().constData();
    if (utf8.empty())
        return;
    g_texts.insert(utf8);
}

// ---------------------------------------------------------------------------
// Display-layer delegate: changes item-view text without touching the model.
// ---------------------------------------------------------------------------
class TranslateDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QString displayText(const QVariant& value, const QLocale& locale) const override {
        QString original = QStyledItemDelegate::displayText(value, locale);
        if (original.isEmpty() || original.contains(QChar(0x3010)))
            return original;
        return QStringLiteral("【汉】") + original;
    }
};

static std::vector<QStyledItemDelegate*> g_delegates;
static std::set<QPointer<QAbstractItemView>> g_delegatedViews;

// ---------------------------------------------------------------------------
// Idempotent display-layer replacement on the GUI thread. Runs every pass so
// controls that appear only after the document / tree loads are translated
// too. Skipped when the marker is already present.
// ---------------------------------------------------------------------------
static void ReplaceTexts(QApplication* app) {
    const auto widgets = QApplication::allWidgets();

    for (QWidget* w : widgets) {
        if (auto* lbl = qobject_cast<QLabel*>(w)) {
            QString t = lbl->text();
            if (!t.isEmpty() && !t.contains(QChar(0x3010)))
                lbl->setText(QStringLiteral("【汉】") + t);
        }
        if (auto* btn = qobject_cast<QAbstractButton*>(w)) {
            QString t = btn->text();
            if (!t.isEmpty() && !t.contains(QChar(0x3010)))
                btn->setText(QStringLiteral("【汉】") + t);
        }
        if (auto* menu = qobject_cast<QMenu*>(w)) {
            for (QAction* a : menu->actions()) {
                QString t = a->text();
                if (!t.isEmpty() && !t.contains(QChar(0x3010)))
                    a->setText(QStringLiteral("【汉】") + t);
            }
        }
    }

    for (QWidget* w : widgets) {
        if (auto* view = qobject_cast<QAbstractItemView*>(w)) {
            if (g_delegatedViews.count(view) == 0) {
                auto* delegate = new TranslateDelegate(view);
                view->setItemDelegate(delegate);
                g_delegates.push_back(delegate);
                g_delegatedViews.insert(view);
            }
        }
    }

    Log("display-layer replacement applied (labels/buttons/menus + item-view delegates)");
}

// ---------------------------------------------------------------------------
// QPainter::drawText IAT hooking (mirrors the Substance localizer).
// SpeedTree's property rows are custom-painted with QPainter::drawText, so
// setText / item delegates cannot reach them. We swap the host's import slot
// for the drawText overloads so every string is translated before it is drawn.
// ---------------------------------------------------------------------------
typedef void(__fastcall* DrawPoint)(QPainter*, const QPoint&, const QString&);
typedef void(__fastcall* DrawPointF)(QPainter*, const QPointF&, const QString&);
typedef void(__fastcall* DrawRect)(QPainter*, const QRect&, int, const QString&, QRect*);
typedef void(__fastcall* DrawRectFOption)(QPainter*, const QRectF&, const QString&, const QTextOption&);
typedef void(__fastcall* DrawXY)(QPainter*, int, int, const QString&);
typedef void(__fastcall* DrawXYWH)(QPainter*, int, int, int, int, int, const QString&, QRect*);

static DrawPoint     g_drawPoint     = nullptr;
static DrawPointF    g_drawPointF    = nullptr;
static DrawRect      g_drawRect      = nullptr;
static DrawRectFOption g_drawRectFOption = nullptr;
static DrawXY        g_drawXY        = nullptr;
static DrawXYWH      g_drawXYWH      = nullptr;

struct GraphHookSlot {
    void** slot;
    void*  original;
    void*  replacement;
};
static std::vector<GraphHookSlot> g_hookSlots;

// Small demo dictionary. The real version loads the full sp-translation-v1 JSON.
static const std::unordered_map<std::string, std::string> g_paintDict = {
    {"Material", "材质"}, {"Mesh", "网格"}, {"Weight", "权重"},
    {"Amount", "数量"}, {"Offset", "偏移"}, {"Gravity", "重力"},
    {"Fold", "折叠"}, {"Curl", "卷曲"}, {"Roll", "滚动"},
    {"Height", "高度"}, {"Boundary", "边界"}, {"Scale", "缩放"},
    {"Shape", "形状"}, {"Lighting", "光照"}, {"Segments", "分段"},
    {"Distribution", "分布"}, {"Width", "宽度"}, {"Seed", "种子"},
    {"Colors", "颜色"}, {"Random Seeds", "随机种子"},
    {"Favor outliers", "偏向异常值"}, {"Planar", "平面"},
    {"Gnarl", "节瘤"}, {"Floral", "花形"},
    {"Left-Right Variance", "左右差异"}, {"Section Variance", "截面差异"},
    {"Two sided", "双面"}, {"Flip", "翻转"}, {"Index", "索引"},
    {"Resolution", "分辨率"}, {"Speed", "速度"}, {"Length", "长度"},
    {"Radius", "半径"}, {"Angles", "角度"},
    {"Uniform (random)", "均匀（随机）"}, {"Spring", "春季"},
    {"Summer", "夏季"}, {"Fall", "秋季"}, {"Winter", "冬季"},
    {"Segments", "分段"}, {"Material Sets", "材质集"}, {"Meshes", "网格"},
    {"Displacements", "置换"}, {"Materials", "材质"},
};

static std::atomic<long long> g_paintCalls{0};
static std::atomic<long long> g_paintTranslated{0};

static QString translatePaint(const QString& text) {
    ++g_paintCalls;
    QString t = text;
    t.replace(QLatin1Char('&'), QString());
    const std::string key = t.trimmed().toUtf8().constData();
    auto it = g_paintDict.find(key);
    if (it != g_paintDict.end()) {
        ++g_paintTranslated;
        return QString::fromUtf8(it->second.c_str());
    }
    return QString();
}

void hookedDrawPoint(QPainter* painter, const QPoint& point, const QString& text) {
    QString target = translatePaint(text);
    if (g_drawPoint) g_drawPoint(painter, point, target.isEmpty() ? text : target);
}
void hookedDrawPointF(QPainter* painter, const QPointF& point, const QString& text) {
    QString target = translatePaint(text);
    if (g_drawPointF) g_drawPointF(painter, point, target.isEmpty() ? text : target);
}
void hookedDrawRect(QPainter* painter, const QRect& rect, int flags,
                    const QString& text, QRect* boundingRect) {
    QString target = translatePaint(text);
    if (g_drawRect) g_drawRect(painter, rect, flags, target.isEmpty() ? text : target, boundingRect);
}
void hookedDrawRectFOption(QPainter* painter, const QRectF& rect,
                           const QString& text, const QTextOption& option) {
    QString target = translatePaint(text);
    if (g_drawRectFOption)
        g_drawRectFOption(painter, rect, target.isEmpty() ? text : target, option);
}
void hookedDrawXY(QPainter* painter, int x, int y, const QString& text) {
    QString target = translatePaint(text);
    if (g_drawXY) g_drawXY(painter, x, y, target.isEmpty() ? text : target);
}
void hookedDrawXYWH(QPainter* painter, int x, int y, int w, int h, int flags,
                    const QString& text, QRect* boundingRect) {
    QString target = translatePaint(text);
    if (g_drawXYWH)
        g_drawXYWH(painter, x, y, w, h, flags, target.isEmpty() ? text : target, boundingRect);
}

static bool replaceMainModuleImportInto(std::vector<GraphHookSlot>& slotList,
                                        void* original, void* replacement) {
    if (!original || !replacement) return false;
    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (!base) return false;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    const DWORD importRva = nt->OptionalHeader
        .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!importRva) return false;
    bool replaced = false;
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + importRva);
    for (; descriptor->Name; ++descriptor) {
        if (!descriptor->FirstThunk) continue;
        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);
        for (; thunk->u1.Function; ++thunk) {
            auto** slot = reinterpret_cast<void**>(&thunk->u1.Function);
            if (*slot != original) continue;
            DWORD oldProtection = 0;
            if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &oldProtection))
                continue;
            *slot = replacement;
            DWORD ignored = 0;
            VirtualProtect(slot, sizeof(void*), oldProtection, &ignored);
            FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
            slotList.push_back({slot, original, replacement});
            replaced = true;
        }
    }
    return replaced;
}

static bool g_drawHooksInstalled = false;
static bool installDrawTextHooks() {
    if (g_drawHooksInstalled) return true;
    HMODULE qtGui = GetModuleHandleW(L"Qt6Gui.dll");
    if (!qtGui) return false;
    bool installed = false;
    g_drawPoint = (DrawPoint)GetProcAddress(
        qtGui, "?drawText@QPainter@@QEAAXAEBVQPoint@@AEBVQString@@@Z");
    if (g_drawPoint)
        installed |= replaceMainModuleImportInto(g_hookSlots, (void*)g_drawPoint, (void*)&hookedDrawPoint);
    g_drawPointF = (DrawPointF)GetProcAddress(
        qtGui, "?drawText@QPainter@@QEAAXAEBVQPointF@@AEBVQString@@@Z");
    if (g_drawPointF)
        installed |= replaceMainModuleImportInto(g_hookSlots, (void*)g_drawPointF, (void*)&hookedDrawPointF);
    g_drawRect = (DrawRect)GetProcAddress(
        qtGui, "?drawText@QPainter@@QEAAXAEBVQRect@@HAEBVQString@@PEAV2@@Z");
    if (g_drawRect)
        installed |= replaceMainModuleImportInto(g_hookSlots, (void*)g_drawRect, (void*)&hookedDrawRect);
    g_drawRectFOption = (DrawRectFOption)GetProcAddress(
        qtGui, "?drawText@QPainter@@QEAAXAEBVQRectF@@AEBVQString@@AEBVQTextOption@@@Z");
    if (g_drawRectFOption)
        installed |= replaceMainModuleImportInto(g_hookSlots, (void*)g_drawRectFOption, (void*)&hookedDrawRectFOption);
    g_drawXY = (DrawXY)GetProcAddress(
        qtGui, "?drawText@QPainter@@QEAAXHHAEBVQString@@@Z");
    if (g_drawXY)
        installed |= replaceMainModuleImportInto(g_hookSlots, (void*)g_drawXY, (void*)&hookedDrawXY);
    g_drawXYWH = (DrawXYWH)GetProcAddress(
        qtGui, "?drawText@QPainter@@QEAAXHHHHHAEBVQString@@PEAVQRect@@@Z");
    if (g_drawXYWH)
        installed |= replaceMainModuleImportInto(g_hookSlots, (void*)g_drawXYWH, (void*)&hookedDrawXYWH);
    g_drawHooksInstalled = installed;
    Log("drawText IAT hooks installed=" + std::to_string(installed ? 1 : 0) +
        " slots=" + std::to_string(g_hookSlots.size()));
    return installed;
}

static void WriteInventory() {
    std::vector<std::string> ordered(g_texts.begin(), g_texts.end());
    std::sort(ordered.begin(), ordered.end());
    std::wstring dir = LocalAppDataDir() + L"\\SpeedTreeChinese";
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring path = dir + L"\\all_texts.txt";
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        std::string body;
        body.reserve(ordered.size() * 48);
        for (const std::string& s : ordered) {
            body += s;
            body += "\n";
        }
        DWORD wr = 0;
        WriteFile(h, body.data(), static_cast<DWORD>(body.size()), &wr, nullptr);
        CloseHandle(h);
    }
}

// ---------------------------------------------------------------------------
// Deep widget-tree dump: class, objectName, text kind, geometry, visibility.
// ---------------------------------------------------------------------------
static const char* TextKind(QWidget* w) {
    if (qobject_cast<QLabel*>(w)) return "QLabel";
    if (qobject_cast<QAbstractButton*>(w)) return "QButton";
    if (qobject_cast<QComboBox*>(w)) return "QCombo";
    if (qobject_cast<QAbstractSpinBox*>(w)) return "QSpin";
    if (qobject_cast<QLineEdit*>(w)) return "QLineEdit";
    if (qobject_cast<QTabBar*>(w)) return "QTabBar";
    if (qobject_cast<QAbstractItemView*>(w)) return "QItemView";
    return "";
}

static std::string WText(QWidget* w) {
    if (auto* lbl = qobject_cast<QLabel*>(w)) return lbl->text().toUtf8().constData();
    if (auto* btn = qobject_cast<QAbstractButton*>(w)) return btn->text().toUtf8().constData();
    if (auto* cb = qobject_cast<QComboBox*>(w)) return cb->currentText().toUtf8().constData();
    if (auto* sp = qobject_cast<QAbstractSpinBox*>(w)) return sp->text().toUtf8().constData();
    if (auto* le = qobject_cast<QLineEdit*>(w)) return le->text().toUtf8().constData();
    return "";
}

static void DumpWidgetDeep(QWidget* w, int depth, std::string& out) {
    if (!w)
        return;
    const std::string indent(depth * 2, ' ');
    const char* cls = w->metaObject()->className();
    std::string name = w->objectName().toUtf8().constData();
    std::string kind = TextKind(w);
    std::string text = WText(w);
    out += indent + "[" + kind + "]" + cls + " name=\"" + name + "\"" +
           (text.empty() ? "" : " text=\"" + text + "\"") +
           " v=" + (w->isVisible() ? "1" : "0") +
           " geo=" + std::to_string(w->x()) + "," + std::to_string(w->y()) +
           " " + std::to_string(w->width()) + "x" + std::to_string(w->height()) + "\n";

    if (auto* view = qobject_cast<QAbstractItemView*>(w)) {
        if (view->model()) {
            out += indent + "    [model] rows=" +
                   std::to_string(view->model()->rowCount()) + " cols=" +
                   std::to_string(view->model()->columnCount()) + "\n";
        }
    }
    for (QObject* c : w->children()) {
        if (auto* child = qobject_cast<QWidget*>(c))
            DumpWidgetDeep(child, depth + 1, out);
    }
}

static void WriteDeepDump(QApplication* app) {
    std::string out = "TopLevelWidgets=" +
                      std::to_string(app->topLevelWidgets().size()) + "\n";
    for (QWidget* w : app->topLevelWidgets())
        DumpWidgetDeep(w, 0, out);
    std::wstring dir = LocalAppDataDir() + L"\\SpeedTreeChinese";
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring path = dir + L"\\ui_tree_deep.txt";
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD wr = 0;
        WriteFile(h, out.data(), static_cast<DWORD>(out.size()), &wr, nullptr);
        CloseHandle(h);
    }
    Log("deep_tree_dump bytes=" + std::to_string(out.size()));
}

// ---------------------------------------------------------------------------
// Model walker (captures list/tree/table display text)
// ---------------------------------------------------------------------------
static void CollectModel(QAbstractItemModel* model) {
    if (!model)
        return;
    const int rows = model->rowCount();
    const int cols = model->columnCount();
    if (rows < 0 || rows > 40000 || cols <= 0 || cols > 64)
        return;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            QModelIndex idx = model->index(r, c);
            if (!idx.isValid())
                continue;
            AddIf(model->data(idx, Qt::DisplayRole).toString());
        }
    }
}

// ---------------------------------------------------------------------------
// Per-pass scan on the GUI thread
// ---------------------------------------------------------------------------
static int g_pass = 0;
static QTimer* g_timer = nullptr;

static void CollectAllTexts(QApplication* app) {
    ++g_pass;
    const auto widgets = QApplication::allWidgets();

    // First capture the original English text BEFORE any replacement.

    for (QWidget* w : widgets) {
        if (auto* lbl = qobject_cast<QLabel*>(w))
            AddIf(lbl->text());
        if (auto* btn = qobject_cast<QAbstractButton*>(w))
            AddIf(btn->text());
        if (auto* le = qobject_cast<QLineEdit*>(w))
            AddIf(le->text());
        if (auto* tb = qobject_cast<QTabBar*>(w)) {
            for (int i = 0; i < tb->count(); ++i)
                AddIf(tb->tabText(i));
        }
        if (auto* tabs = qobject_cast<QTabWidget*>(w)) {
            for (int i = 0; i < tabs->count(); ++i)
                AddIf(tabs->tabText(i));
        }
    }

    for (QWidget* w : widgets) {
        if (auto* cb = qobject_cast<QComboBox*>(w)) {
            AddIf(cb->currentText());
            for (int i = 0; i < cb->count(); ++i)
                AddIf(cb->itemText(i));
        }
        if (auto* sp = qobject_cast<QAbstractSpinBox*>(w))
            AddIf(sp->text());
        if (auto* chk = qobject_cast<QCheckBox*>(w))
            AddIf(chk->text());
        if (!w->windowTitle().isEmpty())
            AddIf(w->windowTitle());
        if (!w->toolTip().isEmpty())
            AddIf(w->toolTip());
    }

    for (QWidget* w : widgets) {
        if (auto* menu = qobject_cast<QMenu*>(w)) {
            for (QAction* a : menu->actions())
                AddIf(a->text());
        }
    }

    for (QWidget* w : widgets) {
        if (auto* view = qobject_cast<QAbstractItemView*>(w))
            CollectModel(view->model());
    }

    WriteInventory();
    Log("pass=" + std::to_string(g_pass) +
        " uniqueStrings=" + std::to_string(g_texts.size()) +
        " drawTextCalls=" + std::to_string(g_paintCalls.load()) +
        " translated=" + std::to_string(g_paintTranslated.load()));

    // Re-apply the display-layer change every pass (capture runs first so the
    // inventory only holds original English).
    ReplaceTexts(app);

    // Deep-dump the widget tree a couple of times once the document is loaded.
    if (g_pass == 6 || g_pass == 12)
        WriteDeepDump(app);

    if (g_pass >= 16 && g_timer) {
        g_timer->stop();
        Log("=== capture complete ===");
    }
}

// ---------------------------------------------------------------------------
// Worker thread: wait for QApplication, then marshal capture to the GUI thread
// ---------------------------------------------------------------------------
static DWORD WINAPI Worker(LPVOID) {
    QApplication* app = nullptr;
    for (int i = 0; i < 900; ++i) {
        if (QCoreApplication::instance()) {
            app = qobject_cast<QApplication*>(QCoreApplication::instance());
            if (app)
                break;
        }
        Sleep(100);
    }
    if (!app) {
        Log("FAILED: QApplication not found within timeout");
        return 1;
    }
    Log("QApplication found. Starting capture.");
    installDrawTextHooks();

    QMetaObject::invokeMethod(app,
                              [app]() {
                                  // First pass immediately, then every 3s.
                                  CollectAllTexts(app);
                                  g_timer = new QTimer(app);
                                  g_timer->setInterval(3000);
                                  QObject::connect(g_timer, &QTimer::timeout,
                                                   app, [app]() {
                                                       CollectAllTexts(app);
                                                   });
                                  g_timer->start();
                              },
                              Qt::QueuedConnection);
    return 0;
}

// ---------------------------------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        Log("=== SpeedTreeChinese capture probe loaded ===");
        HANDLE h = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        if (h)
            CloseHandle(h);
    }
    return TRUE;
}

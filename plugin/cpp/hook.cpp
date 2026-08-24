// ============================================================================
//  SpeedTree Modeler Chinese Localizer - Translation Engine (Hook DLL)
// ============================================================================
//  Architecture mirrors the Substance 3D Painter/Designer localizer, adapted
//  to run standalone because SpeedTree exposes no Python plug-in API:
//
//  Dictionaries  sp-translation-v1 JSON in <plug-in>/translations/*_zh.json,
//                loaded sorted & merged (later files win).
//  Lookup        1. global exact  2. fuzzy match.
//  Display       1. QStyledItemDelegate::displayText for list/tree/table
//                2. QPainter::drawText IAT hook for every painted control
//  Layout        3. QFontMetrics/F measurement hooks so sizeHint/eliding see
//                   Chinese glyph bounds while widget/model text stays English.
//  Data model    Never rewritten: no setText/setTabText/setItemText mutations.
//  Controls      F10 enable/disable, F12 write untranslated strings.
//  API           C exports so a future settings UI / updater can version it.
//
//  Delivery      launcher.cpp starts SpeedTree_Modeler.exe SUSPENDED, injects
//                this DLL, then resumes. Nothing on disk is modified.
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <atomic>
#include <set>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <utility>
#include <functional>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QLocale>
#include <QtCore/QTimer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QPointer>
#include <QtCore/QModelIndex>
#include <QtCore/QAbstractItemModel>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtWidgets/QAction>
#else
#include <QtGui/QAction>
#endif
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <QtGui/QTextOption>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QCloseEvent>
#include <QtGui/QKeySequence>
#include <QtGui/QContextMenuEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QLabel>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtGui/QDesktopServices>
#include <QtCore/QUrl>

// ---------------------------------------------------------------------------
// Version / identity
// ---------------------------------------------------------------------------
extern "C" __declspec(dllexport) int __cdecl st_localizer_api_version() { return 1; }
extern "C" __declspec(dllexport) const char* __cdecl st_localizer_version() {
    return "0.1.0";
}

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
static bool      g_enabled          = true;
static int       g_toggleKey        = Qt::Key_F2;    // 中/英 toggle hotkey (default F2)
static QString   g_plugInDir;
static HMODULE   g_selfModule       = nullptr;
static bool      g_hooksInstalled   = false;

static std::unordered_map<std::string, std::string> g_translations;       // global
static std::unordered_map<std::string, std::string> g_idTranslations;     // control-ID
static std::unordered_map<std::string, std::string> g_foldedTranslations; // fuzzy index
static std::unordered_map<std::string, std::string> g_normCache;          // text -> normalized
static constexpr size_t kMaxNormCache = 100000;
static int g_patchedSlots = 0;                     // drawText IAT slots replaced

static std::set<std::string> g_paintSeen;          // untranslated drawText strings
static bool     g_sniffActive  = false;             // Shift+` 1.5s sniff window
static QTimer*  g_sniffTimer   = nullptr;

static QString normalizeForMatch(QString text);
static bool containsCjk(const QString& text);
static void installAuthorLink();
static void repaintAll();
static void refreshLayouts();
static void runUiTour();
static bool replaceImports(HMODULE module, void* original, void* replacement);

// ---------------------------------------------------------------------------
// Paths / logging
// ---------------------------------------------------------------------------
static std::wstring SelfDirW() {
    std::vector<wchar_t> buf(1024);
    while (buf.size() <= 32768) {
        DWORD n = GetModuleFileNameW(g_selfModule, buf.data(),
                                     static_cast<DWORD>(buf.size()));
        if (!n) return L"";
        if (n < buf.size() - 1) {
            std::wstring path(buf.data(), n);
            size_t slash = path.find_last_of(L"\\/");
            return slash == std::wstring::npos ? L"" : path.substr(0, slash);
        }
        buf.resize(buf.size() * 2);
    }
    return L"";
}

static void Log(const std::string& msg) {
    std::wstring dir = SelfDirW();
    std::wstring path = dir + L"\\localizer.log";
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
// Minimal JSON reader for the flat sp-translation-v1 "translations" object.
// ---------------------------------------------------------------------------
static bool ParseJsonString(const char*& p, std::string& out) {
    if (*p != '"') return false;
    ++p;
    out.clear();
    while (*p && *p != '"') {
        if (static_cast<unsigned char>(*p) < 0x20) return false;
        if (*p == '\\') {
            ++p;
            switch (*p) {
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/');  break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'u': {
                    unsigned int cp = 0;
                    for (int i = 0; i < 4; ++i) {
                        char c = p[1 + i];
                        cp <<= 4;
                        if (c >= '0' && c <= '9') cp |= (c - '0');
                        else if (c >= 'a' && c <= 'f') cp |= (c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F') cp |= (c - 'A' + 10);
                        else return false;
                    }
                    p += 4;
                    // Encode UTF-8 (BMP only; surrogate pairs are uncommon here).
                    if (cp < 0x80) out.push_back(char(cp));
                    else if (cp < 0x800) {
                        out.push_back(char(0xC0 | (cp >> 6)));
                        out.push_back(char(0x80 | (cp & 0x3F)));
                    } else {
                        out.push_back(char(0xE0 | (cp >> 12)));
                        out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
                        out.push_back(char(0x80 | (cp & 0x3F)));
                    }
                    break;
                }
                default: return false;
            }
            ++p;
        } else {
            out.push_back(*p);
            ++p;
        }
    }
    if (*p != '"') return false;
    ++p;
    return true;
}

static void ParseTranslations(const std::string& body,
                              std::unordered_map<std::string, std::string>& out) {
    size_t pos = body.find("\"translations\"");
    if (pos == std::string::npos) return;
    pos = body.find('{', pos);
    if (pos == std::string::npos) return;
    const char* p = body.c_str() + pos + 1;
    while (*p) {
        if (*p == '}') break;                       // end of translations object
        if (*p == '"') {
            std::string key, value;
            if (ParseJsonString(p, key)) {
                while (*p && std::isspace((unsigned char)*p)) ++p;
                if (*p == ':') {
                    ++p;
                    while (*p && std::isspace((unsigned char)*p)) ++p;
                    if (ParseJsonString(p, value)) {
                        // Empty value = placeholder, not loaded (substance rule).
                        if (!value.empty() && !key.empty())
                            out[key] = value;
                    }
                }
            }
        } else {
            ++p;
        }
    }
}

static std::string ReadFileBytes(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QByteArray data = f.readAll();
    return std::string(data.constData(), data.size());
}

static void LoadDictionaries() {
    g_translations.clear();
    g_idTranslations.clear();
    g_foldedTranslations.clear();
    g_normCache.clear();
    QDir dir(g_plugInDir + "/translations");
    if (!dir.exists()) return;
    QStringList files = dir.entryList({"*_zh.json"}, QDir::Files, QDir::Name);
    for (const QString& name : files) {
        std::string body = ReadFileBytes(dir.filePath(name));
        if (body.empty()) continue;
        std::unordered_map<std::string, std::string> map;
        ParseTranslations(body, map);
        for (const auto& kv : map) {
            if (kv.first.find("||") != std::string::npos)
                g_idTranslations[kv.first] = kv.second;
            else
                g_translations[kv.first] = kv.second;
        }
    }
    // Build the fuzzy index (normalized English key -> Chinese).
    for (const auto& kv : g_translations)
        g_foldedTranslations[normalizeForMatch(QString::fromUtf8(kv.first.c_str()))
                                 .toUtf8().constData()] = kv.second;
    Log("dictionaries loaded: global=" + std::to_string(g_translations.size()) +
        " id=" + std::to_string(g_idTranslations.size()));
}

// ---------------------------------------------------------------------------
// i18n normalization + fuzzy lookup (mirrors the Substance localizer)
// ---------------------------------------------------------------------------
static QString normalizeForMatch(QString text) {
    // Cached: the same English source is normalised only the first time it is
    // seen, so repeated draws of untranslated text don't re-run this pipeline.
    const std::string in = text.toUtf8().constData();
    auto cached = g_normCache.find(in);
    if (cached != g_normCache.end())
        return QString::fromUtf8(cached->second.c_str());

    text.replace(QChar(0x00A0), QLatin1Char(' '));
    text.replace(QChar(0x3000), QLatin1Char(' '));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    text = text.simplified();
    text.remove(QChar(0x200B)); text.remove(QChar(0x200D)); text.remove(QChar(0xFEFF));
    while (text.endsWith(QChar(0x2026))) text.chop(1);
    while (text.endsWith(QLatin1String("..."))) text.chop(3);
    if (text.endsWith(QLatin1String(" *"))) text.chop(2);
    text.replace(QLatin1Char('_'), QLatin1Char(' '));
    text = text.normalized(QString::NormalizationForm_C);
    for (int i = 0; i < text.size(); ++i) {
        const uint code = text.at(i).unicode();
        if (code >= 0xFF01 && code <= 0xFF5E) text[i] = QChar(code - 0xFEE0);
    }
    text = text.toCaseFolded();
    text = text.normalized(QString::NormalizationForm_D);
    QString stripped; stripped.reserve(text.size());
    for (const QChar ch : text) {
        const QChar::Category cat = ch.category();
        if (cat == QChar::Mark_NonSpacing || cat == QChar::Mark_SpacingCombining ||
            cat == QChar::Mark_Enclosing)
            continue;
        stripped.append(ch);
    }
    stripped.remove(QLatin1Char(' '));
    QString result = stripped.normalized(QString::NormalizationForm_C);
    if (g_normCache.size() < kMaxNormCache)
        g_normCache[in] = result.toUtf8().constData();
    return result;
}

static bool containsCjk(const QString& text) {
    for (QChar ch : text) {
        const ushort u = ch.unicode();
        if ((u >= 0x3400 && u <= 0x9FFF) || (u >= 0xF900 && u <= 0xFAFF))
            return true;
    }
    return false;
}

// JSON-string escape for a UTF-8 key/value (quotes, backslash, control chars).
static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    const char* hex = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(hex[(c >> 4) & 0xF]);
                    out.push_back(hex[c & 0xF]);
                } else {
                    out.push_back((char)c);
                }
        }
    }
    return out;
}

// Plain display-layer lookup: exact first, then fuzzy.
static QString translated(const QString& raw) {
    if (!g_enabled) return {};
    QString key = raw.trimmed();
    if (key.isEmpty()) return {};

    auto exact = g_translations.find(key.toUtf8().constData());
    if (exact != g_translations.end())
        return QString::fromUtf8(exact->second.c_str());
    if (containsCjk(key)) return {};

    // mnemonic / state suffix handling
    QString stateSuffix;
    if (key.endsWith(QLatin1Char('*'))) { key.chop(1); key = key.trimmed(); stateSuffix = "*"; }
    if (key.contains(QLatin1Char('&'))) key.remove(QLatin1Char('&'));
    auto found = g_translations.find(key.toUtf8().constData());
    if (found != g_translations.end())
        return QString::fromUtf8(found->second.c_str()) + stateSuffix;

    const std::string norm = normalizeForMatch(key).toUtf8().constData();
    auto fuzzy = g_foldedTranslations.find(norm);
    if (fuzzy != g_foldedTranslations.end())
        return QString::fromUtf8(fuzzy->second.c_str()) + stateSuffix;
    return {};
}

// ---------------------------------------------------------------------------
// Item-view delegate: display-layer translation for list/tree/table.
// ---------------------------------------------------------------------------
class TranslateDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QString displayText(const QVariant& value, const QLocale& locale) const override {
        QString original = QStyledItemDelegate::displayText(value, locale);
        if (original.isEmpty() || containsCjk(original)) return original;
        QString t = translated(original);
        return t.isEmpty() ? original : t;
    }
};

static std::vector<QPointer<QStyledItemDelegate>> g_delegates;
static std::set<QPointer<QAbstractItemView>> g_delegatedViews;

// ---------------------------------------------------------------------------
// Item-view delegation: list/tree/table items are translated via the delegate.
// Everything else is translated only at QPainter::drawText call time.
// ---------------------------------------------------------------------------
static void applyWidget(QWidget* w);

static void applyTranslations(QApplication* app) {
    if (!g_enabled) return;
    for (QWidget* w : QApplication::allWidgets())
        applyWidget(w);
}

// ---------------------------------------------------------------------------
// CElidingLabel (Qt Advanced Docking) read-only full-text access.
// Its tooltip retains the full source while QLabel::text() may be elided.
// ---------------------------------------------------------------------------
static bool isElidingLabel(QWidget* w) {
    return w && QString::fromLatin1(w->metaObject()->className()) ==
                    QLatin1String("ads::CElidingLabel");
}

// Full source text for a label, masking Qt Advanced Docking eliding labels.
static QString labelSource(QLabel* lbl) {
    if (!lbl) return {};
    QString t = lbl->text();
    // ads::CElidingLabel keeps the FULL source in the toolTip (set by its own
    // setText) while QLabel::text() holds only the elided fragment drawn
    // (e.g. "Mater…" for "Material").  Reading the toolTip therefore recovers
    // the complete string for dictionary lookup without a by-value call.
    if (isElidingLabel(lbl)) {
        QString tip = lbl->toolTip();
        if (!tip.isEmpty()) return tip;
        return t;
    }
    // Generic eliding-QLabel fallback: the full source lives in the toolTip.
    if (t.isEmpty()) return t;
    bool elided = t.endsWith(QChar(0x2026)) || t.endsWith(QLatin1String("..."));
    QString tip = lbl->toolTip();
    if (elided && !tip.isEmpty() && tip.size() > t.size()) {
        int chop = t.endsWith(QChar(0x2026)) ? 1 : 3;
        if (tip.startsWith(t.left(t.size() - chop)))
            return tip;
    }
    return t;
}

// Flip the 中/英 state. Because the experiment never rewrites widget/model
// strings, toggling only schedules repainting; there is nothing to restore.
static void toggleTranslation() {
    g_enabled = !g_enabled;
    refreshLayouts();
    repaintAll();
    Log("toggle enabled=" + std::to_string(g_enabled ? 1 : 0));
}

// Install only the display-layer delegate. Standard controls are handled by
// drawText hooks, leaving their text properties and sizeHint inputs untouched.
// Called from the event filter on Show / UpdateRequest / Paint as controls appear.
static void applyWidget(QWidget* w) {
    if (!w || !g_enabled) return;
    if (auto* view = qobject_cast<QAbstractItemView*>(w)) {
        if (g_delegatedViews.count(view) == 0) {
            auto* d = new TranslateDelegate(view);
            view->setItemDelegate(d);
            g_delegates.push_back(d);
            g_delegatedViews.insert(view);
        }
    }
}

// ---------------------------------------------------------------------------
// F10 toggle, F12 sniffer (one application event filter)
// ---------------------------------------------------------------------------
static bool isTranslatableString(const QString& s) {
    QString t = s.trimmed();
    if (t.isEmpty() || containsCjk(t)) return false;
    if (t.size() < 2) return false;
    if (t[0] == '<') return false;                 // HTML
    if (t.contains(QLatin1Char('?'))) return false; // URL-ish
    // numeric detection
    bool numeric = true;
    for (QChar c : t) {
        if (!(c.isDigit() || c == '.' || c == ',' || c == '%' || c == '-' ||
              c == '+' || c == 'e' || c == 'E' || c == ' ')) { numeric = false; break; }
    }
    return !numeric;
}

// ---- untranslated-word sniffer (Shift+` starts a 1.5 s window) --------------
static void collectWidgetStrings(std::map<std::string, std::string>& pending) {
    const auto widgets = QApplication::allWidgets();
    auto add = [&](const QString& s) {
        if (!isTranslatableString(s)) return;
        std::string k = s.trimmed().toUtf8().constData();
        if (k.empty() || g_translations.count(k) || g_idTranslations.count(k)) return;
        pending.emplace(k, "");
    };
    for (QWidget* w : widgets) {
        if (auto* lbl = qobject_cast<QLabel*>(w)) add(labelSource(lbl));
        if (auto* btn = qobject_cast<QAbstractButton*>(w)) add(btn->text());
        if (auto* le = qobject_cast<QLineEdit*>(w)) add(le->text());
        if (auto* cb = qobject_cast<QComboBox*>(w)) {
            add(cb->currentText());
            for (int i = 0; i < cb->count(); ++i) add(cb->itemText(i));
        }
        if (auto* menu = qobject_cast<QMenu*>(w))
            for (QAction* a : menu->actions()) add(a->text());
        if (auto* tb = qobject_cast<QTabBar*>(w))
            for (int i = 0; i < tb->count(); ++i) add(tb->tabText(i));
        if (auto* view = qobject_cast<QAbstractItemView*>(w)) {
            if (view->model()) {
                auto* model = view->model();
                int rows = model->rowCount(), cols = model->columnCount();
                if (rows > 0 && rows < 40000 && cols > 0 && cols < 64) {
                    for (int r = 0; r < rows; ++r)
                        for (int c2 = 0; c2 < cols; ++c2)
                            add(model->data(model->index(r, c2), Qt::DisplayRole).toString());
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Automatic UI tour (ST_TOUR=1): open-ish pass over the whole live UI so every
// visible / model-driven string is captured without relying on the manual 1.5 s
// sniffer.  Walks all widgets, every menu + submenu + toolbar action, tabs,
// combo items, line edits, tooltips, and every item-view model row/column
// (this is where node parameters live), then also merges the drawText strings
// seen since launch.  Writes the missing (untranslated) strings to
// <plugin>\translations\tour_untranslated_zh.json.
// ---------------------------------------------------------------------------
static bool g_tourActive = false;

// Open/expand the UI so lazy content is created, capturing after each step.
// Runs on the GUI thread.  Safe: only expands views and cycles tabs/combos,
// restoring the previous selection afterwards.  Context menus / modal dialogs
// are NOT auto-triggered here (they can block); they are captured on Show.
static void autoOpenPanels() {
    // 1) Expand every tree view to materialize lazy node rows / parameters.
    for (QWidget* w : QApplication::allWidgets())
        if (auto* tv = qobject_cast<QTreeView*>(w)) tv->expandAll();
    QApplication::processEvents();
    runUiTour();

    // 2) Cycle each standard tab bar (normal Qt tabs); dock tabs are handled by
    //    switching the underlying CDockWidgetTab widgets below.
    for (QWidget* w : QApplication::allWidgets()) {
        auto* tb = qobject_cast<QTabBar*>(w);
        if (!tb || tb->count() < 2) continue;
        const int cur = tb->currentIndex();
        for (int i = 0; i < tb->count(); ++i) {
            tb->setCurrentIndex(i);
            QApplication::processEvents();
            runUiTour();
        }
        tb->setCurrentIndex(cur);
        QApplication::processEvents();
    }

    // 3) Cycle every combo box to reveal dependent controls (restore after).
    for (QWidget* w : QApplication::allWidgets()) {
        auto* cb = qobject_cast<QComboBox*>(w);
        if (!cb || cb->count() < 2) continue;
        const int cur = cb->currentIndex();
        for (int i = 0; i < cb->count(); ++i) {
            cb->setCurrentIndex(i);
            QApplication::processEvents();
            runUiTour();
        }
        cb->setCurrentIndex(cur);
        QApplication::processEvents();
    }
}

// --- menu capture: any QMenu shown via exec/popup is recorded (untranslated),
//     and while the auto-tour is active context menus are auto-closed so the
//     synthetic right-click triggering below never blocks the GUI thread.
typedef QAction* (__fastcall* QMenuExecP)(void*, const QPoint&, QAction*);
typedef QAction* (__fastcall* QMenuExec0)(void*);
typedef void     (__fastcall* QMenuPopup)(void*, const QPoint&, QAction*);
static QMenuExecP  g_menuExecP  = nullptr;
static QMenuExec0  g_menuExec0  = nullptr;
static QMenuPopup  g_menuPopup  = nullptr;
static bool        g_menuHooked = false;
static long        g_menuCaptured = 0;

static void captureMenu(QMenu* m) {
    if (!m) return;
    for (QAction* a : m->actions()) {
        if (!a->text().isEmpty()) {
            if (!isTranslatableString(a->text())) continue;
            std::string k = a->text().trimmed().toUtf8().constData();
            if (k.empty() || g_translations.count(k) || g_idTranslations.count(k)) continue;
            if (g_paintSeen.size() < 200000) {
                g_paintSeen.insert(k);
                ++g_menuCaptured;
            }
        }
        if (a->menu()) captureMenu(a->menu());
    }
}

static void closeMenuSoon(QMenu* m) {
    QTimer::singleShot(80, m, [m] { m->close(); });
}

static QAction* hookedQMenuExecP(void* _this, const QPoint& pos, QAction* act) {
    auto* m = static_cast<QMenu*>(_this);
    if (g_tourActive && m) { captureMenu(m); closeMenuSoon(m); }
    return g_menuExecP ? g_menuExecP(m, pos, act) : nullptr;
}
static QAction* hookedQMenuExec0(void* _this) {
    auto* m = static_cast<QMenu*>(_this);
    if (g_tourActive && m) { captureMenu(m); closeMenuSoon(m); }
    return g_menuExec0 ? g_menuExec0(m) : nullptr;
}
static void hookedQMenuPopup(void* _this, const QPoint& pos, QAction* act) {
    auto* m = static_cast<QMenu*>(_this);
    if (g_tourActive && m) { captureMenu(m); closeMenuSoon(m); }
    if (g_menuPopup) g_menuPopup(m, pos, act);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
static const wchar_t* kGuiModule = L"Qt5Gui.dll";
static const wchar_t* kCoreModule = L"Qt5Core.dll";
static const wchar_t* kWidgetsModule = L"Qt5Widgets.dll";
static const wchar_t* kHookDllPrefix = L"SpeedTreeChineseHook";
#else
static const wchar_t* kGuiModule = L"Qt6Gui.dll";
static const wchar_t* kCoreModule = L"Qt6Core.dll";
static const wchar_t* kWidgetsModule = L"Qt6Widgets.dll";
static const wchar_t* kHookDllPrefix = L"SpeedTreeChineseHook";
#endif

static void installMenuHooks() {
    if (g_menuHooked) return;
    HMODULE w = GetModuleHandleW(kWidgetsModule);
    if (!w) return;
    g_menuExecP = (QMenuExecP)GetProcAddress(w,
        "?exec@QMenu@@QEAAPEAVQAction@@AEBVQPoint@@PEAV2@@Z");
    g_menuExec0 = (QMenuExec0)GetProcAddress(w,
        "?exec@QMenu@@QEAAPEAVQAction@@XZ");
    g_menuPopup = (QMenuPopup)GetProcAddress(w,
        "?popup@QMenu@@QEAAXAEBVQPoint@@PEAVQAction@@@Z");
    struct MSym { void* orig; void* hook; };
    MSym syms[] = {
        {(void*)g_menuExecP, (void*)&hookedQMenuExecP},
        {(void*)g_menuExec0, (void*)&hookedQMenuExec0},
        {(void*)g_menuPopup, (void*)&hookedQMenuPopup},
    };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W mod{}; mod.dwSize = sizeof(mod);
        if (Module32FirstW(snap, &mod)) {
            do {
                if (_wcsnicmp(mod.szModule, kHookDllPrefix, wcslen(kHookDllPrefix)) == 0) continue;
                HMODULE h = GetModuleHandleW(mod.szModule);
                if (!h) continue;
                for (auto& s : syms)
                    if (s.orig) replaceImports(h, s.orig, s.hook);
            } while (Module32NextW(snap, &mod));
        }
        CloseHandle(snap);
    }
    g_menuHooked = true;
}

// Synthetically right-click each tree view / large panel so its context menu is
// created, captured by the exec/popup hook, then auto-closed.
static void walkRows(QAbstractItemView* view, const QModelIndex& parent,
                     int depth, int& budget) {
    if (budget <= 0 || depth > 4 || !view->model()) return;
    QAbstractItemModel* m = view->model();
    int rows = m->rowCount(parent);
    for (int r = 0; r < rows && budget > 0; ++r) {
        QModelIndex idx = m->index(r, 0, parent);
        if (!idx.isValid()) continue;
        --budget;
        view->setCurrentIndex(idx);
        const QRect rect = view->visualRect(idx);
        if (!rect.isEmpty()) {
            QPoint pos = rect.center();
            QContextMenuEvent ev(QContextMenuEvent::Mouse, pos,
                                 view->mapToGlobal(pos),
                                 QApplication::keyboardModifiers());
            QApplication::sendEvent(view, &ev);
            QApplication::processEvents();
        }
        walkRows(view, idx, depth + 1, budget);
    }
}

static void triggerContextMenus() {
    const auto widgets = QApplication::allWidgets();
    int views = 0;
    for (QWidget* w : widgets) {
        if (!w || !w->isVisible()) continue;
        // Item views (node/property trees, lists, tables) own the useful context
        // menus; skip generic widgets to avoid opening unintended dialogs.
        auto* view = qobject_cast<QAbstractItemView*>(w);
        if (!view || !view->model()) continue;
        ++views;
        int budget = 60;
        int before = (int)g_paintSeen.size();
        walkRows(view, QModelIndex(), 0, budget);
        int captured = (int)g_paintSeen.size() - before;
        Log("tour view '" + std::string(view->metaObject()->className()) +
            "' captured=" + std::to_string(captured));
    }
    Log("tour triggerContextMenus: views=" + std::to_string(views) +
        " newMenuStrings=" + std::to_string(g_menuCaptured));
}

static void collectMenuActions(QMenu* menu, std::function<void(const QString&)> add) {
    if (!menu) return;
    for (QAction* a : menu->actions()) {
        if (!a->text().isEmpty()) add(a->text());
        if (a->menu()) collectMenuActions(a->menu(), add);
    }
}

static void runUiTour() {
    std::map<std::string, std::string> pending;
    auto add = [&](const QString& s) {
        if (!isTranslatableString(s)) return;
        std::string k = s.trimmed().toUtf8().constData();
        if (k.empty() || g_translations.count(k) || g_idTranslations.count(k)) return;
        pending.emplace(k, "");
    };
    const auto widgets = QApplication::allWidgets();
    for (QWidget* w : widgets) {
        if (auto* lbl = qobject_cast<QLabel*>(w)) add(labelSource(lbl));
        if (auto* btn = qobject_cast<QAbstractButton*>(w)) add(btn->text());
        if (auto* le = qobject_cast<QLineEdit*>(w)) add(le->text());
        if (auto* cb = qobject_cast<QComboBox*>(w)) {
            add(cb->currentText());
            for (int i = 0; i < cb->count(); ++i) add(cb->itemText(i));
        }
        if (auto* tb = qobject_cast<QTabBar*>(w))
            for (int i = 0; i < tb->count(); ++i) add(tb->tabText(i));
        if (!w->toolTip().isEmpty()) add(w->toolTip());
        if (auto* menu = qobject_cast<QMenu*>(w))
            collectMenuActions(menu, add);
        if (auto* mb = qobject_cast<QMenuBar*>(w))
            for (QAction* a : mb->actions()) {
                add(a->text());
                if (a->menu()) collectMenuActions(a->menu(), add);
            }
        if (auto* tb2 = qobject_cast<QToolBar*>(w))
            for (QAction* a : tb2->actions()) add(a->text());
        if (auto* view = qobject_cast<QAbstractItemView*>(w)) {
            if (view->model()) {
                auto* model = view->model();
                int rows = model->rowCount(), cols = model->columnCount();
                if (rows > 0 && rows < 100000 && cols > 0 && cols < 64) {
                    for (int r = 0; r < rows; ++r)
                        for (int c = 0; c < cols; ++c)
                            add(model->data(model->index(r, c), Qt::DisplayRole).toString());
                }
            }
        }
    }
    // drawText / custom-painted strings captured since launch.
    for (const std::string& s : g_paintSeen)
        if (!s.empty() && !g_translations.count(s))
            pending.emplace(s, "");

    std::string json = "{\n  \"$schema\": \"sp-translation-v1\",\n"
                       "  \"id\": \"speedtree-tour\",\n"
                       "  \"language\": \"zh-CN\",\n"
                       "  \"description\": \"UI 巡检采集的未翻译词条（待填写）\",\n"
                       "  \"translations\": {\n";
    bool first = true;
    for (const auto& kv : pending) {
        if (!first) json += ",\n";
        json += "    \"" + jsonEscape(kv.first) + "\": \"\""; first = false;
    }
    json += "\n  }\n}\n";
    std::wstring dir = SelfDirW() + L"\\translations";
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring path = dir + L"\\tour_untranslated_zh.json";
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD wr = 0;
        WriteFile(h, json.data(), (DWORD)json.size(), &wr, nullptr);
        CloseHandle(h);
    }
    Log("UI tour wrote " + std::to_string(pending.size()) + " untranslated strings");
}

static void writeUntranslated(const std::map<std::string, std::string>& pending) {
    std::string json = "{\n  \"$schema\": \"sp-translation-v1\",\n"
                       "  \"id\": \"speedtree-sniffed\",\n"
                       "  \"language\": \"zh-CN\",\n"
                       "  \"description\": \"未翻译词条（Shift+` 采集，待填写）\",\n"
                       "  \"translations\": {\n";
    bool first = true;
    for (const auto& kv : pending) {
        if (!first) json += ",\n";
        std::string ekey;
        for (char c : kv.first) { if (c == '"' || c == '\\') ekey += '\\'; ekey += c; }
        json += "    \"" + ekey + "\": \"\""; first = false;
    }
    json += "\n  }\n}\n";
    std::wstring dir = SelfDirW() + L"\\translations";
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring path = dir + L"\\untranslated_zh.json";
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD wr = 0;
        WriteFile(h, json.data(), (DWORD)json.size(), &wr, nullptr);
        CloseHandle(h);
    }
}

static void openSnifferDir() {
    std::wstring dir = SelfDirW() + L"\\translations";
    CreateDirectoryW(dir.c_str(), nullptr);
    ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static void finishSniffer() {
    g_sniffActive = false;
    std::map<std::string, std::string> pending;
    collectWidgetStrings(pending);
    for (const std::string& s : g_paintSeen)
        if (!s.empty() && !g_translations.count(s)) pending.emplace(s, "");
    writeUntranslated(pending);
    g_paintSeen.clear();
    Log("sniffer done, wrote " + std::to_string(pending.size()) + " entries");
    openSnifferDir();
}

static void startSniffer() {
    if (g_sniffActive) return;
    g_sniffActive = true;
    g_paintSeen.clear();
    if (!g_sniffTimer) {
        g_sniffTimer = new QTimer(QCoreApplication::instance());
        g_sniffTimer->setSingleShot(true);
        QObject::connect(g_sniffTimer, &QTimer::timeout, [] { finishSniffer(); });
    }
    g_sniffTimer->start(1500);
    Log("sniffer ON (1.5s)");
}

class TranslatorFilter : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject* object, QEvent* event) override {
        if (!g_enabled && event->type() != QEvent::KeyPress) return false;
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == g_toggleKey) {
                toggleTranslation();
                return true;
            }
            if ((ke->key() == Qt::Key_QuoteLeft || ke->key() == Qt::Key_AsciiTilde) &&
                (ke->modifiers() & Qt::ShiftModifier)) {
                startSniffer();
                return true;
            }
            return false;
        }
        if (event->type() == QEvent::Show || event->type() == QEvent::UpdateRequest ||
            event->type() == QEvent::Paint) {
            if (auto* w = qobject_cast<QWidget*>(object))
                applyWidget(w);
            if (object && qobject_cast<QMenuBar*>(object) &&
                event->type() == QEvent::Show)
                installAuthorLink();
            // Capture any menu that pops up (incl. right-click context menus):
            // record its (untranslated) action texts so the UI tour picks them up.
            if (event->type() == QEvent::Show) {
                if (auto* menu = qobject_cast<QMenu*>(object))
                    collectMenuActions(menu, [](const QString& s) {
                        if (!isTranslatableString(s)) return;
                        std::string k = s.trimmed().toUtf8().constData();
                        if (k.empty() || g_translations.count(k) ||
                            g_idTranslations.count(k)) return;
                        if (g_paintSeen.size() < 200000) g_paintSeen.insert(k);
                    });
            }
        }
        return false;
    }
};

// ---------------------------------------------------------------------------
// QPainter::drawText IAT hook (custom-painted text) -- mirrors Substance.
// ---------------------------------------------------------------------------
typedef void(__fastcall* DrawPoint)(QPainter*, const QPoint&, const QString&);
typedef void(__fastcall* DrawPointF)(QPainter*, const QPointF&, const QString&);
typedef void(__fastcall* DrawRect)(QPainter*, const QRect&, int, const QString&, QRect*);
typedef void(__fastcall* DrawRectFOption)(QPainter*, const QRectF&, const QString&, const QTextOption&);
typedef void(__fastcall* DrawRectFAlign)(QPainter*, const QRectF&, int, const QString&, QRect*);
typedef void(__fastcall* DrawXY)(QPainter*, int, int, const QString&);
typedef void(__fastcall* DrawXYWH)(QPainter*, int, int, int, int, int, const QString&, QRect*);
typedef void(__fastcall* DrawPointF2)(QPainter*, const QPointF&, const QString&, int, int);

static DrawPoint     g_drawPoint     = nullptr;
static DrawPointF    g_drawPointF    = nullptr;
static DrawRect      g_drawRect      = nullptr;
static DrawRectFOption g_drawRectFOption = nullptr;
static DrawRectFAlign g_drawRectFAlign = nullptr;
static DrawXY        g_drawXY        = nullptr;
static DrawXYWH      g_drawXYWH      = nullptr;
static DrawPointF2   g_drawPointF2   = nullptr;

static QString translatePaint(const QString& text) {
    if (text.isEmpty() || containsCjk(text)) return {};
    // Cheap reject for values that can never be UI text (numbers, tiny tokens),
    // so the expensive fuzzy-normalization path is not hit on every draw.
    if (text.size() <= 1) return {};
    bool allValue = true;
    for (QChar c : text) {
        if (c.isDigit() || c == QLatin1Char('.') || c == QLatin1Char(',') ||
            c == QLatin1Char('%') || c == QLatin1Char('-') || c == QLatin1Char('+') ||
            c == QLatin1Char('e') || c == QLatin1Char('E') || c == QLatin1Char(' '))
            continue;
        allValue = false; break;
    }
    if (allValue) return {};
    QString tr = translated(text);
    if (!tr.isEmpty()) return tr;
    // Record untranslated paint strings (custom-drawn property labels, etc.).
    if ((g_sniffActive || g_tourActive) && g_paintSeen.size() < 100000 &&
        isTranslatableString(text)) {
        std::string k = text.trimmed().toUtf8().constData();
        if (!k.empty()) g_paintSeen.insert(k);
    }
    return {};
}

static void hookedDrawPoint(QPainter* p, const QPoint& pt, const QString& t) {
    QString tr = translatePaint(t);
    if (g_drawPoint) g_drawPoint(p, pt, tr.isEmpty() ? t : tr);
}
static void hookedDrawPointF(QPainter* p, const QPointF& pt, const QString& t) {
    QString tr = translatePaint(t);
    if (g_drawPointF) g_drawPointF(p, pt, tr.isEmpty() ? t : tr);
}
static void hookedDrawRect(QPainter* p, const QRect& r, int f, const QString& t, QRect* br) {
    QString tr = translatePaint(t);
    if (g_drawRect) g_drawRect(p, r, f, tr.isEmpty() ? t : tr, br);
}
static void hookedDrawRectFOption(QPainter* p, const QRectF& r, const QString& t, const QTextOption& o) {
    QString tr = translatePaint(t);
    if (g_drawRectFOption) g_drawRectFOption(p, r, tr.isEmpty() ? t : tr, o);
}
static void hookedDrawRectFAlign(QPainter* p, const QRectF& r, int f, const QString& t, QRect* br) {
    QString tr = translatePaint(t);
    if (g_drawRectFAlign) g_drawRectFAlign(p, r, f, tr.isEmpty() ? t : tr, br);
}
static void hookedDrawXY(QPainter* p, int x, int y, const QString& t) {
    QString tr = translatePaint(t);
    if (g_drawXY) g_drawXY(p, x, y, tr.isEmpty() ? t : tr);
}
static void hookedDrawXYWH(QPainter* p, int x, int y, int w, int h, int f, const QString& t, QRect* br) {
    QString tr = translatePaint(t);
    if (g_drawXYWH) g_drawXYWH(p, x, y, w, h, f, tr.isEmpty() ? t : tr, br);
}
static void hookedDrawPointF2(QPainter* p, const QPointF& pt, const QString& t, int f, int d) {
    QString tr = translatePaint(t);
    if (g_drawPointF2) g_drawPointF2(p, pt, tr.isEmpty() ? t : tr, f, d);
}

// Patch every import slot in `module` that points at `original`.
static bool replaceImports(HMODULE module, void* original, void* replacement) {
    auto* base = reinterpret_cast<unsigned char*>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    const DWORD importRva = nt->OptionalHeader
        .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!importRva) return false;
    bool replaced = false;
    auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + importRva);
    for (; desc->Name; ++desc) {
        if (!desc->FirstThunk) continue;
        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);
        for (; thunk->u1.Function; ++thunk) {
            void** slot = reinterpret_cast<void**>(&thunk->u1.Function);
            if (*slot != original) continue;
            DWORD old = 0;
            if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) continue;
            *slot = replacement;
            DWORD ign = 0;
            VirtualProtect(slot, sizeof(void*), old, &ign);
            FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
            ++g_patchedSlots;
            replaced = true;
        }
    }
    return replaced;
}

// ---------------------------------------------------------------------------
// QFontMetrics measurement hooks.
//
// Qt layouts calculate sizeHint from the widget's ORIGINAL English text. The
// pure-paint path translates only at drawText time, so the layout can leave too
// little space for the wider Chinese rendering. Mirroring Toolbag's approach,
// translate the string at the font-measurement boundary instead: the widget
// keeps English internally while layout/eliding sees the Chinese glyph bounds.
// ---------------------------------------------------------------------------
typedef int (__fastcall* FmHAdv)(const QFontMetrics*, const QString&, int);
typedef int (__fastcall* FmHAdvOpt)(const QFontMetrics*, const QString&, const QTextOption&);
// MSVC x64 returns QRect/QSize/QString through a hidden sret pointer. The
// function pointer types below model the raw ABI explicitly: RCX = this,
// RDX = sret, then the ordinary Qt arguments in registers/stack.
typedef void* (__fastcall* FmBRectRaw)(const QFontMetrics* self, QRect* result, const QString& text);
typedef void* (__fastcall* FmBRectOptRaw)(const QFontMetrics* self, QRect* result, const QString& text, const QTextOption& opt);
typedef void* (__fastcall* FmBRectRectRaw)(const QFontMetrics* self, QRect* result, const QRect& rect, int flags, const QString& text, int tabstops, int* tabarray);
typedef void* (__fastcall* FmSizeRaw)(const QFontMetrics* self, QSize* result, int flags, const QString& text, int tabstops, int* tabarray);
typedef void* (__fastcall* FmTBRectRaw)(const QFontMetrics* self, QRect* result, const QString& text);
typedef void* (__fastcall* FmTBRectOptRaw)(const QFontMetrics* self, QRect* result, const QString& text, const QTextOption& opt);
typedef void* (__fastcall* FmElideRaw)(const QFontMetrics* self, QString* result, const QString& text, Qt::TextElideMode mode, int width, int flags);

typedef qreal (__fastcall* FmFHAdv)(const QFontMetricsF*, const QString&, int);
typedef qreal (__fastcall* FmFHAdvOpt)(const QFontMetricsF*, const QString&, const QTextOption&);
typedef void* (__fastcall* FmFBRectRaw)(const QFontMetricsF* self, QRectF* result, const QString& text);
typedef void* (__fastcall* FmFBRectOptRaw)(const QFontMetricsF* self, QRectF* result, const QString& text, const QTextOption& opt);
typedef void* (__fastcall* FmFBRectRectRaw)(const QFontMetricsF* self, QRectF* result, const QRectF& rect, int flags, const QString& text, int tabstops, int* tabarray);
typedef void* (__fastcall* FmFSizeRaw)(const QFontMetricsF* self, QSizeF* result, int flags, const QString& text, int tabstops, int* tabarray);
typedef void* (__fastcall* FmFTBRectRaw)(const QFontMetricsF* self, QRectF* result, const QString& text);
typedef void* (__fastcall* FmFTBRectOptRaw)(const QFontMetricsF* self, QRectF* result, const QString& text, const QTextOption& opt);
typedef void* (__fastcall* FmFElideRaw)(const QFontMetricsF* self, QString* result, const QString& text, Qt::TextElideMode mode, qreal width, int flags);

static FmHAdv     g_fmHAdv      = nullptr;
static FmHAdvOpt  g_fmHAdvOpt   = nullptr;
static FmBRectRaw g_fmBRect     = nullptr;
static FmBRectOptRaw g_fmBRectOpt = nullptr;
static FmBRectRectRaw g_fmBRectRect = nullptr;
static FmSizeRaw  g_fmSize      = nullptr;
static FmTBRectRaw g_fmTBRect   = nullptr;
static FmTBRectOptRaw g_fmTBRectOpt = nullptr;
static FmElideRaw g_fmElide     = nullptr;
static FmFHAdv    g_fmfHAdv     = nullptr;
static FmFHAdvOpt g_fmfHAdvOpt  = nullptr;
static FmFBRectRaw g_fmfBRect   = nullptr;
static FmFBRectOptRaw g_fmfBRectOpt = nullptr;
static FmFBRectRectRaw g_fmfBRectRect = nullptr;
static FmFSizeRaw g_fmfSize     = nullptr;
static FmFTBRectRaw g_fmfTBRect = nullptr;
static FmFTBRectOptRaw g_fmfTBRectOpt = nullptr;
static FmFElideRaw g_fmfElide   = nullptr;
static bool       g_fontHooksInstalled = false;

static QString measureText(const QString& text) {
    if (text.isEmpty() || containsCjk(text)) return {};
    return translated(text);
}

static int hookedFontHAdv(const QFontMetrics* fm, const QString& text, int len) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (len >= 0 && len > t.size()) len = static_cast<int>(t.size());
    if (g_fmHAdv) return g_fmHAdv(fm, t, len);
    return 0;
}
static int hookedFontHAdvOpt(const QFontMetrics* fm, const QString& text, const QTextOption& opt) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmHAdvOpt) return g_fmHAdvOpt(fm, t, opt);
    return 0;
}
static void* hookedFontBRect(const QFontMetrics* fm, QRect* result, const QString& text) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmBRect) g_fmBRect(fm, result, t);
    return result;
}
static void* hookedFontBRectOpt(const QFontMetrics* fm, QRect* result, const QString& text, const QTextOption& opt) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmBRectOpt) g_fmBRectOpt(fm, result, t, opt);
    return result;
}
static void* hookedFontBRectRect(const QFontMetrics* fm, QRect* result, const QRect& r, int flags, const QString& text, int tabstops, int* tabarray) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmBRectRect) g_fmBRectRect(fm, result, r, flags, t, tabstops, tabarray);
    return result;
}
static void* hookedFontSize(const QFontMetrics* fm, QSize* result, int flags, const QString& text, int tabstops, int* tabarray) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmSize) g_fmSize(fm, result, flags, t, tabstops, tabarray);
    return result;
}
static void* hookedFontTBRect(const QFontMetrics* fm, QRect* result, const QString& text) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmTBRect) g_fmTBRect(fm, result, t);
    return result;
}
static void* hookedFontTBRectOpt(const QFontMetrics* fm, QRect* result, const QString& text, const QTextOption& opt) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmTBRectOpt) g_fmTBRectOpt(fm, result, t, opt);
    return result;
}
static void* hookedFontElide(const QFontMetrics* fm, QString* result, const QString& text, Qt::TextElideMode mode, int width, int flags) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmElide) g_fmElide(fm, result, t, mode, width, flags);
    else *result = t;
    return result;
}

static qreal hookedFontFHAdv(const QFontMetricsF* fm, const QString& text, int len) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (len >= 0 && len > t.size()) len = static_cast<int>(t.size());
    if (g_fmfHAdv) return g_fmfHAdv(fm, t, len);
    return 0;
}
static qreal hookedFontFHAdvOpt(const QFontMetricsF* fm, const QString& text, const QTextOption& opt) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmfHAdvOpt) return g_fmfHAdvOpt(fm, t, opt);
    return 0;
}
static void* hookedFontFBRect(const QFontMetricsF* fm, QRectF* result, const QString& text) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmfBRect) g_fmfBRect(fm, result, t);
    return result;
}
static void* hookedFontFBRectOpt(const QFontMetricsF* fm, QRectF* result, const QString& text, const QTextOption& opt) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmfBRectOpt) g_fmfBRectOpt(fm, result, t, opt);
    return result;
}
static void* hookedFontFBRectRect(const QFontMetricsF* fm, QRectF* result, const QRectF& r, int flags, const QString& text, int tabstops, int* tabarray) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmfBRectRect) g_fmfBRectRect(fm, result, r, flags, t, tabstops, tabarray);
    return result;
}
static void* hookedFontFSize(const QFontMetricsF* fm, QSizeF* result, int flags, const QString& text, int tabstops, int* tabarray) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmfSize) g_fmfSize(fm, result, flags, t, tabstops, tabarray);
    return result;
}
static void* hookedFontFTBRect(const QFontMetricsF* fm, QRectF* result, const QString& text) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmfTBRect) g_fmfTBRect(fm, result, t);
    return result;
}
static void* hookedFontFTBRectOpt(const QFontMetricsF* fm, QRectF* result, const QString& text, const QTextOption& opt) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmfTBRectOpt) g_fmfTBRectOpt(fm, result, t, opt);
    return result;
}
static void* hookedFontFElide(const QFontMetricsF* fm, QString* result, const QString& text, Qt::TextElideMode mode, qreal width, int flags) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmfElide) g_fmfElide(fm, result, t, mode, width, flags);
    else *result = t;
    return result;
}

static void installFontMeasurementHooks() {
    if (g_fontHooksInstalled) return;
    HMODULE qtGui = GetModuleHandleW(kGuiModule);
    if (!qtGui) return;

    g_fmHAdv = (FmHAdv)GetProcAddress(qtGui,
        "?horizontalAdvance@QFontMetrics@@QEBAHAEBVQString@@H@Z");
    g_fmHAdvOpt = (FmHAdvOpt)GetProcAddress(qtGui,
        "?horizontalAdvance@QFontMetrics@@QEBAHAEBVQString@@AEBVQTextOption@@@Z");
    g_fmBRect = (FmBRectRaw)GetProcAddress(qtGui,
        "?boundingRect@QFontMetrics@@QEBA?AVQRect@@AEBVQString@@@Z");
    g_fmBRectOpt = (FmBRectOptRaw)GetProcAddress(qtGui,
        "?boundingRect@QFontMetrics@@QEBA?AVQRect@@AEBVQString@@AEBVQTextOption@@@Z");
    g_fmBRectRect = (FmBRectRectRaw)GetProcAddress(qtGui,
        "?boundingRect@QFontMetrics@@QEBA?AVQRect@@AEBV2@HAEBVQString@@HPEAH@Z");
    g_fmSize = (FmSizeRaw)GetProcAddress(qtGui,
        "?size@QFontMetrics@@QEBA?AVQSize@@HAEBVQString@@HPEAH@Z");
    g_fmTBRect = (FmTBRectRaw)GetProcAddress(qtGui,
        "?tightBoundingRect@QFontMetrics@@QEBA?AVQRect@@AEBVQString@@@Z");
    g_fmTBRectOpt = (FmTBRectOptRaw)GetProcAddress(qtGui,
        "?tightBoundingRect@QFontMetrics@@QEBA?AVQRect@@AEBVQString@@AEBVQTextOption@@@Z");
    g_fmElide = (FmElideRaw)GetProcAddress(qtGui,
        "?elidedText@QFontMetrics@@QEBA?AVQString@@AEBV2@W4TextElideMode@Qt@@HH@Z");

    g_fmfHAdv = (FmFHAdv)GetProcAddress(qtGui,
        "?horizontalAdvance@QFontMetricsF@@QEBANAEBVQString@@H@Z");
    g_fmfHAdvOpt = (FmFHAdvOpt)GetProcAddress(qtGui,
        "?horizontalAdvance@QFontMetricsF@@QEBANAEBVQString@@AEBVQTextOption@@@Z");
    g_fmfBRect = (FmFBRectRaw)GetProcAddress(qtGui,
        "?boundingRect@QFontMetricsF@@QEBA?AVQRectF@@AEBVQString@@@Z");
    g_fmfBRectOpt = (FmFBRectOptRaw)GetProcAddress(qtGui,
        "?boundingRect@QFontMetricsF@@QEBA?AVQRectF@@AEBVQString@@AEBVQTextOption@@@Z");
    g_fmfBRectRect = (FmFBRectRectRaw)GetProcAddress(qtGui,
        "?boundingRect@QFontMetricsF@@QEBA?AVQRectF@@AEBV2@HAEBVQString@@HPEAH@Z");
    g_fmfSize = (FmFSizeRaw)GetProcAddress(qtGui,
        "?size@QFontMetricsF@@QEBA?AVQSizeF@@HAEBVQString@@HPEAH@Z");
    g_fmfTBRect = (FmFTBRectRaw)GetProcAddress(qtGui,
        "?tightBoundingRect@QFontMetricsF@@QEBA?AVQRectF@@AEBVQString@@@Z");
    g_fmfTBRectOpt = (FmFTBRectOptRaw)GetProcAddress(qtGui,
        "?tightBoundingRect@QFontMetricsF@@QEBA?AVQRectF@@AEBVQString@@AEBVQTextOption@@@Z");
    g_fmfElide = (FmFElideRaw)GetProcAddress(qtGui,
        "?elidedText@QFontMetricsF@@QEBA?AVQString@@AEBV2@W4TextElideMode@Qt@@NH@Z");

    // Raw ABI hooks: every struct-return is modelled with an explicit sret
    // first parameter (RCX), matching the Qt6Gui DLL's MSVC x64 calling
    // convention. This covers QLabel/QMenu/QComboBox's boundingRect paths,
    // QPushButton/QTabBar/QMenuBar's size paths, and eliding.
    struct Sym { const char* name; void* orig; void* hook; };
    Sym syms[] = {
        {"FMHAdv",      (void*)g_fmHAdv,      (void*)&hookedFontHAdv},
        {"FMHAdvOpt",   (void*)g_fmHAdvOpt,   (void*)&hookedFontHAdvOpt},
        {"FMBRect",     (void*)g_fmBRect,     (void*)&hookedFontBRect},
        {"FMBRectOpt",  (void*)g_fmBRectOpt,  (void*)&hookedFontBRectOpt},
        {"FMBRectRect", (void*)g_fmBRectRect, (void*)&hookedFontBRectRect},
        {"FMSize",      (void*)g_fmSize,      (void*)&hookedFontSize},
        {"FMTBRect",    (void*)g_fmTBRect,    (void*)&hookedFontTBRect},
        {"FMTBRectOpt", (void*)g_fmTBRectOpt, (void*)&hookedFontTBRectOpt},
        {"FMElide",     (void*)g_fmElide,     (void*)&hookedFontElide},
        {"FMfHAdv",     (void*)g_fmfHAdv,     (void*)&hookedFontFHAdv},
        {"FMfHAdvOpt",  (void*)g_fmfHAdvOpt,  (void*)&hookedFontFHAdvOpt},
        {"FMfBRect",    (void*)g_fmfBRect,    (void*)&hookedFontFBRect},
        {"FMfBRectOpt", (void*)g_fmfBRectOpt, (void*)&hookedFontFBRectOpt},
        {"FMfBRectRect",(void*)g_fmfBRectRect,(void*)&hookedFontFBRectRect},
        {"FMfSize",     (void*)g_fmfSize,     (void*)&hookedFontFSize},
        {"FMfTBRect",   (void*)g_fmfTBRect,   (void*)&hookedFontFTBRect},
        {"FMfTBRectOpt",(void*)g_fmfTBRectOpt,(void*)&hookedFontFTBRectOpt},
        {"FMfElide",    (void*)g_fmfElide,    (void*)&hookedFontFElide},
    };

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W mod{}; mod.dwSize = sizeof(mod);
        if (Module32FirstW(snap, &mod)) {
            do {
                if (_wcsnicmp(mod.szModule, kHookDllPrefix, wcslen(kHookDllPrefix)) == 0) continue;
                if (_wcsicmp(mod.szModule, kCoreModule) == 0) continue;
                HMODULE h = GetModuleHandleW(mod.szModule);
                if (!h) continue;
                for (auto& s : syms) {
                    if (s.orig) replaceImports(h, s.orig, s.hook);
                }
            } while (Module32NextW(snap, &mod));
        }
        CloseHandle(snap);
    }
    g_fontHooksInstalled = true;
    Log("font measurement IAT hooks installed; patchedSlots=" + std::to_string(g_patchedSlots));
}

static void installDrawTextHooks() {
    if (g_hooksInstalled) return;
    HMODULE qtGui = GetModuleHandleW(kGuiModule);
    if (!qtGui) return;

    struct Sym { const char* name; void* orig; void* hook; };
    g_drawPoint = (DrawPoint)GetProcAddress(qtGui,
        "?drawText@QPainter@@QEAAXAEBVQPoint@@AEBVQString@@@Z");
    g_drawPointF = (DrawPointF)GetProcAddress(qtGui,
        "?drawText@QPainter@@QEAAXAEBVQPointF@@AEBVQString@@@Z");
    g_drawRect = (DrawRect)GetProcAddress(qtGui,
        "?drawText@QPainter@@QEAAXAEBVQRect@@HAEBVQString@@PEAV2@@Z");
    g_drawRectFOption = (DrawRectFOption)GetProcAddress(qtGui,
        "?drawText@QPainter@@QEAAXAEBVQRectF@@AEBVQString@@AEBVQTextOption@@@Z");
    g_drawRectFAlign = (DrawRectFAlign)GetProcAddress(qtGui,
        "?drawText@QPainter@@QEAAXAEBVQRectF@@HAEBVQString@@PEAV2@@Z");
    g_drawXY = (DrawXY)GetProcAddress(qtGui,
        "?drawText@QPainter@@QEAAXHHAEBVQString@@@Z");
    g_drawXYWH = (DrawXYWH)GetProcAddress(qtGui,
        "?drawText@QPainter@@QEAAXHHHHHAEBVQString@@PEAVQRect@@@Z");
    g_drawPointF2 = (DrawPointF2)GetProcAddress(qtGui,
        "?drawText@QPainter@@QEAAXAEBVQPointF@@AEBVQString@@HH@Z");

    Sym syms[] = {
        {"Point",     (void*)g_drawPoint,     (void*)&hookedDrawPoint},
        {"PointF",    (void*)g_drawPointF,    (void*)&hookedDrawPointF},
        {"Rect",      (void*)g_drawRect,      (void*)&hookedDrawRect},
        {"RectF",     (void*)g_drawRectFOption,(void*)&hookedDrawRectFOption},
        {"RectFAlign",(void*)g_drawRectFAlign, (void*)&hookedDrawRectFAlign},
        {"XY",        (void*)g_drawXY,        (void*)&hookedDrawXY},
        {"XYWH",      (void*)g_drawXYWH,      (void*)&hookedDrawXYWH},
        {"PointF2",   (void*)g_drawPointF2,   (void*)&hookedDrawPointF2},
    };

    // Patch every loaded non-Qt module's imports (the host exe + SpeedTree DLLs).
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W mod{}; mod.dwSize = sizeof(mod);
        if (Module32FirstW(snap, &mod)) {
            do {
                // Patch every module EXCEPT our own hook DLL and Qt6Core. We
                // MUST include Qt6Widgets/Gui: QComboBox and other Qt-styled
                // widgets draw their text through Qt's own QPainter::drawText,
                // which would otherwise escape translation.
                if (_wcsnicmp(mod.szModule, kHookDllPrefix, wcslen(kHookDllPrefix)) == 0) continue;
                if (_wcsicmp(mod.szModule, kCoreModule) == 0) continue;
                HMODULE h = GetModuleHandleW(mod.szModule);
                if (!h) continue;
                for (auto& s : syms) {
                    if (s.orig) replaceImports(h, s.orig, s.hook);
                }
            } while (Module32NextW(snap, &mod));
        }
        CloseHandle(snap);
    }
    g_hooksInstalled = true;
    Log("drawText IAT hooks installed; patchedSlots=" + std::to_string(g_patchedSlots));
}

// ---------------------------------------------------------------------------
// GUI-thread worker: waits for QApplication, then installs the engine.
// ---------------------------------------------------------------------------
static const QString kAuthorUrl = QStringLiteral("https://space.bilibili.com/281243426?spm_id_from=333.1007.0.0");
static const QString kGitHubUrl = QStringLiteral("https://github.com/iillya/SpeedTreeChinese");

static QString keyName(int key) {
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12)
        return QStringLiteral("F") + QString::number(key - Qt::Key_F1 + 1);
    switch (key) {
        case Qt::Key_Space: return QStringLiteral("Space");
        case Qt::Key_Escape: return QStringLiteral("Esc");
        case Qt::Key_Return: case Qt::Key_Enter: return QStringLiteral("Enter");
        case Qt::Key_Tab: return QStringLiteral("Tab");
        default: return QKeySequence(key).toString(QKeySequence::NativeText);
    }
}

static void repaintAll() {
    for (QWidget* w : QApplication::topLevelWidgets()) w->update();
}

struct WidthReserve {
    QPointer<QWidget> widget;
    int originalMin = 0;
};
static std::vector<WidthReserve> g_widthReserve;

static WidthReserve* findWidthReserve(QWidget* w) {
    for (auto& e : g_widthReserve)
        if (e.widget == w) return &e;
    return nullptr;
}

static int scalarFontWidth(const QFontMetrics& fm, const QString& text) {
    if (g_fmHAdv)
        return g_fmHAdv(&fm, text, -1);
    return fm.horizontalAdvance(text);
}

static int neededWidth(QWidget* w) {
    int need = 0;
    if (auto* lbl = qobject_cast<QLabel*>(w)) {
        QString src = labelSource(lbl);
        QString tr = translated(src);
        if (!tr.isEmpty() && tr != src) {
            QFontMetrics fm(lbl->font());
            int srcW = scalarFontWidth(fm, src);
            int trW = scalarFontWidth(fm, tr);
            int hint = lbl->sizeHint().width();
            int base = hint - srcW;
            if (base < 0) base = 0;
            need = base + trW;
        }
    } else if (auto* btn = qobject_cast<QAbstractButton*>(w)) {
        QString src = btn->text();
        QString tr = translated(src);
        if (!tr.isEmpty() && tr != src) {
            QFontMetrics fm(btn->font());
            int srcW = scalarFontWidth(fm, src);
            int trW = scalarFontWidth(fm, tr);
            int hint = btn->sizeHint().width();
            int base = hint - srcW;
            if (base < 0) base = 0;
            need = base + trW;
        }
    } else if (auto* tb = qobject_cast<QTabBar*>(w)) {
        QFontMetrics fm(tb->font());
        int hint = tb->sizeHint().width();
        int extra = 0;
        for (int i = 0; i < tb->count(); ++i) {
            QString src = tb->tabText(i);
            QString tr = translated(src);
            if (!tr.isEmpty() && tr != src)
                extra += scalarFontWidth(fm, tr) - scalarFontWidth(fm, src);
        }
        if (extra > 0) need = hint + extra;
    } else if (auto* cb = qobject_cast<QComboBox*>(w)) {
        QFontMetrics fm(cb->font());
        int hint = cb->sizeHint().width();
        int extra = 0;
        for (int i = 0; i < cb->count(); ++i) {
            QString src = cb->itemText(i);
            QString tr = translated(src);
            if (!tr.isEmpty() && tr != src)
                extra = qMax(extra, scalarFontWidth(fm, tr) - scalarFontWidth(fm, src));
        }
        if (extra > 0) need = hint + extra;
    }
    return need;
}

static void applyWidthReserve(QWidget* w) {
    WidthReserve* e = findWidthReserve(w);
    int orig = e ? e->originalMin : w->minimumWidth();
    if (!e) {
        g_widthReserve.push_back({w, orig});
        e = &g_widthReserve.back();
    }
    int need = neededWidth(w);
    int target = (g_enabled && need > orig) ? need : orig;
    if (w->minimumWidth() != target)
        w->setMinimumWidth(target);
}

static void refreshLayouts() {
    for (QWidget* w : QApplication::allWidgets()) {
        // QMenuBar's internal layout uses cached action geometries and does not
        // tolerate a blanket invalidate during startup; invalidating it can make
        // actionGeometry() return garbage and blank the top menu bar. Leave menu
        // bars/menus alone and let their own show/paint pass re-layout them.
        if (qobject_cast<QMenuBar*>(w) || qobject_cast<QMenu*>(w))
            continue;
        applyWidthReserve(w);
        if (w) w->updateGeometry();
        if (w && w->layout()) w->layout()->invalidate();
    }
}

class ToggleHotkeyDialog : public QDialog {
public:
    explicit ToggleHotkeyDialog(QWidget* parent) : QDialog(parent) {
        setWindowTitle(QStringLiteral("设置 中/英 切换快捷键"));
        setAttribute(Qt::WA_DeleteOnClose);
        auto* lay = new QVBoxLayout(this);
        lay->addWidget(new QLabel(
            QStringLiteral("点击下方按钮后按下新快捷键（Esc 取消）："), this));
        capBtn_ = new QPushButton(QStringLiteral("当前：") + keyName(g_toggleKey), this);
        auto* ok = new QPushButton(QStringLiteral("确定"), this);
        auto* reset = new QPushButton(QStringLiteral("重置"), this);
        lay->addWidget(capBtn_);
        auto* row = new QHBoxLayout();
        row->addWidget(ok);
        row->addWidget(reset);
        lay->addLayout(row);
        // Tidy button highlight state:
        //   - OK is the default button (Enter accepts) -> gets the default ring.
        //   - Reset / capture are NOT default buttons, so they are not auto-highlighted.
        //   - Initial focus goes to OK so Reset never appears focused on open.
        ok->setDefault(true);
        ok->setAutoDefault(true);
        reset->setAutoDefault(false);
        capBtn_->setAutoDefault(false);
        row->setSpacing(8);
        setFocusProxy(ok);
        setStyleSheet(QStringLiteral(
            "QPushButton { background:#3f3f3f; color:#e6e6e6; border:1px solid #565656;"
            " border-radius:4px; padding:6px 16px; }"
            "QPushButton:hover { background:#4c4c4c; }"
            "QPushButton:pressed { background:#333333; }"
            "QPushButton:default { border:1px solid #66aaff; }"));
        QObject::connect(capBtn_, &QPushButton::clicked, this, [this] { startListen(); });
        QObject::connect(ok, &QPushButton::clicked, this, [this] { stopListen(); accept(); });
        QObject::connect(reset, &QPushButton::clicked, this, [this] {
            g_toggleKey = Qt::Key_F2;
            stopListen();
            capBtn_->setText(QStringLiteral("当前：") + keyName(g_toggleKey));
        });
    }
private:
    QPushButton* capBtn_;
    bool listening_ = false;
    void startListen() {
        listening_ = true;
        QApplication::instance()->installEventFilter(this);
        capBtn_->setText(QStringLiteral("请按下新的快捷键…"));
    }
    void stopListen() {
        if (listening_) {
            listening_ = false;
            QApplication::instance()->removeEventFilter(this);
        }
        capBtn_->setText(QStringLiteral("当前：") + keyName(g_toggleKey));
    }
    bool eventFilter(QObject*, QEvent* e) override {
        if (!listening_) return false;
        if (e->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(e);
            if (ke->key() == Qt::Key_Escape) { stopListen(); return true; }
            g_toggleKey = ke->key();
            stopListen();
            return true;
        }
        return false;
    }
    void reject() override { stopListen(); QDialog::reject(); }
    void closeEvent(QCloseEvent* e) override { stopListen(); QDialog::closeEvent(e); }
};

static QPointer<QWidget>      g_authorLinkBox;
static QPointer<QMenuBar>     g_authorLinkBar;
static QPointer<QAction>      g_authorHelpAction;

static void layoutAuthorLinkBox() {
    if (!g_authorLinkBox || !g_authorLinkBar || !g_authorHelpAction) return;
    const QRect hr = g_authorLinkBar->actionGeometry(g_authorHelpAction);
    if (hr.isEmpty()) return;
    const int w = g_authorLinkBox->sizeHint().width();
    const int h = g_authorLinkBar->height();
    // Vertically fill the menu bar row so the two links sit on the same
    // vertical middle as the menu items. Horizontal stays just after 中/英.
    const int x = hr.right() + 12;
    g_authorLinkBox->setGeometry(x, 0, w, h);
    g_authorLinkBox->setFixedHeight(h);
    g_authorLinkBox->raise();
    g_authorLinkBox->show();
}

class AuthorLinkResizeFilter : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (obj && ev->type() == QEvent::Resize) layoutAuthorLinkBox();
        return false;
    }
};

static QPointer<QAction> g_langAction;

static void doToggleLanguage() {
    toggleTranslation();
}

static void openHotkeyDialog() {
    static QPointer<ToggleHotkeyDialog> g_hotkeyDlg;
    if (g_hotkeyDlg && g_hotkeyDlg->isVisible()) {    // already open -> just focus it
        g_hotkeyDlg->raise();
        g_hotkeyDlg->activateWindow();
        return;
    }
    QWidget* parent = g_authorLinkBar ? g_authorLinkBar->window() : nullptr;
    auto* dlg = new ToggleHotkeyDialog(parent);
    g_hotkeyDlg = dlg;
    QObject::connect(dlg, &QObject::destroyed, [] { g_hotkeyDlg = nullptr; });
    dlg->show(); dlg->raise();
}

class LangMouseFilter : public QObject {
public:
    LangMouseFilter(QObject* p) : QObject(p) {}
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (obj && ev->type() == QEvent::MouseButtonPress && g_langAction) {
            if (!ev->spontaneous()) return false;           // ignore synthetic events
            auto* me = static_cast<QMouseEvent*>(ev);
            auto* mb = qobject_cast<QMenuBar*>(obj);
            if (mb && mb->actionGeometry(g_langAction).contains(me->pos())) {
                if (me->button() == Qt::RightButton) {
                    openHotkeyDialog();
                    return true;                       // consume right-click on 中/英
                }
                if (me->button() == Qt::LeftButton) {
                    pressed_ = true;
                    return true;                       // consume left-click on 中/英
                }
            }
        } else if (obj && ev->type() == QEvent::MouseButtonRelease && g_langAction && pressed_) {
            if (!ev->spontaneous()) return false;           // ignore synthetic events
            auto* me = static_cast<QMouseEvent*>(ev);
            auto* mb = qobject_cast<QMenuBar*>(obj);
            if (me->button() == Qt::LeftButton) {
                pressed_ = false;
                if (mb && mb->actionGeometry(g_langAction).contains(me->pos()))
                    doToggleLanguage();                 // fire on release over 中/英
                return true;
            }
        }
        return false;
    }
private:
    bool pressed_ = false;
};

static void installAuthorLink() {
    if (g_authorLinkBar && g_authorLinkBox) return;   // already installed on this bar
    QMenuBar* mb = nullptr;
    for (QWidget* w : QApplication::allWidgets()) {
        if (auto* menu = qobject_cast<QMenuBar*>(w)) {
            if (menu->actions().size() >= 4) { mb = menu; break; }
        }
    }
    if (!mb) return;

    // Locate Help so the attribution sits right after it.
    QAction* helpAction = nullptr;
    for (QAction* a : mb->actions()) {
        QString t = a->text(); t.remove(QLatin1Char('&'));
        if (t.contains(QStringLiteral("帮助")) || t.contains(QStringLiteral("Help"))) {
            helpAction = a; break;
        }
    }
    if (!helpAction) return;

    // "中/英" as a real menu-bar action -> inherits the exact Help item style.
    QAction* langAct = new QAction(QStringLiteral("中/英"), mb);
    langAct->setObjectName(QStringLiteral("speedtree_lang"));
    g_langAction = langAct;
    mb->installEventFilter(new LangMouseFilter(mb));   // right-click -> hotkey dialog
    const auto acts = mb->actions();
    int idx = helpAction ? acts.indexOf(helpAction) : -1;
    QAction* after = (idx >= 0 && idx + 1 < acts.size()) ? acts[idx + 1] : nullptr;
    if (after) mb->insertAction(after, langAct); else mb->addAction(langAct);

    // Author + GitHub as positioned labels (placed after the "中/英" item).
    auto* box = new QWidget(mb);
    box->setObjectName(QStringLiteral("speedtree_author"));
    auto* lay = new QHBoxLayout(box);
    lay->setContentsMargins(4, 0, 4, 0);
    lay->setSpacing(10);

    auto* author = new QLabel(
        QStringLiteral("<a href=\"%1\" style=\"color:#66aaff;text-decoration:underline;\">Bilibili神说要凑数汉化</a>")
            .arg(kAuthorUrl), box);
    auto* github = new QLabel(
        QStringLiteral("<a href=\"%1\" style=\"color:#66aaff;text-decoration:underline;\">Github仓库</a>")
            .arg(kGitHubUrl), box);
    for (QLabel* l : {author, github}) {
        l->setTextFormat(Qt::RichText);
        l->setOpenExternalLinks(false);
        l->setTextInteractionFlags(Qt::TextBrowserInteraction);
        l->setFocusPolicy(Qt::NoFocus);   // no focus rectangle around the link
        l->setCursor(Qt::PointingHandCursor);
    }
    QObject::connect(author, &QLabel::linkActivated,
                     [](const QString& u) { if (!u.isEmpty()) QDesktopServices::openUrl(QUrl(u)); });
    QObject::connect(github, &QLabel::linkActivated,
                     [](const QString& u) { if (!u.isEmpty()) QDesktopServices::openUrl(QUrl(u)); });
    lay->addWidget(author, 0, Qt::AlignVCenter);
    lay->addWidget(github, 0, Qt::AlignVCenter);

    g_authorLinkBox = box;
    g_authorLinkBar = mb;
    g_authorHelpAction = langAct;          // position author links after 中/英
    mb->installEventFilter(new AuthorLinkResizeFilter(mb));
    QTimer::singleShot(0, mb, [] { layoutAuthorLinkBox(); });
    layoutAuthorLinkBox();
    Log("中/英 menu action inserted after Help; author links after it");
}

static void RunOnGuiThread(QApplication* app) {
    // Automatic UI tour: ST_TOUR=1 collects every live UI string at two points
    // after launch, so late-loading panels are captured too.
    const char* tour = std::getenv("ST_TOUR");
    if (tour && *tour == '1') {
        g_tourActive = true;
        installMenuHooks();
        // Phase 1: expand trees (+ capture), then cycle tabs & combos (+ capture)
        QTimer::singleShot(4000, app, [] { autoOpenPanels(); });
        // Phase 2: synthetically right-click item views to open & capture their
        // context menus (auto-closed by the exec/popup hook).
        QTimer::singleShot(12000, app, [] { triggerContextMenus(); runUiTour(); });
        auto* tourTimer = new QTimer(app);
        tourTimer->setInterval(5000);
        QObject::connect(tourTimer, &QTimer::timeout, [] { runUiTour(); });
        tourTimer->start();
        Log("UI tour enabled (ST_TOUR=1): auto-open + context-menu capture");
    }
    LoadDictionaries();
    applyTranslations(app);
    installDrawTextHooks();
    installFontMeasurementHooks();
    refreshLayouts();
    Log("display mode=pure-paint (widget/model text remains original)");
    app->installEventFilter(new TranslatorFilter(app));
    installAuthorLink();
    Log("ui installed; enabled=" + std::to_string(g_enabled ? 1 : 0));

}

static DWORD WINAPI Worker(LPVOID) {
    QApplication* app = nullptr;
    for (int i = 0; i < 900; ++i) {
        if (QCoreApplication::instance()) {
            app = qobject_cast<QApplication*>(QCoreApplication::instance());
            if (app) break;
        }
        Sleep(100);
    }
    if (!app) { Log("FAILED: QApplication not found"); return 1; }
    QMetaObject::invokeMethod(app, [app](){ RunOnGuiThread(app); }, Qt::QueuedConnection);
    return 0;
}

// ---------------------------------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        g_selfModule = hinst;
        g_plugInDir = QString::fromStdWString(SelfDirW());
        Log("=== SpeedTree Chinese Localizer v0.1.0 loaded ===");
        HANDLE h = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    return TRUE;
}

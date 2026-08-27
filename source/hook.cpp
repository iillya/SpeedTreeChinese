// ============================================================================
//  SpeedTree Modeler Chinese Localizer - Translation Engine (Hook DLL)
// ============================================================================
//  Architecture mirrors the Substance 3D Painter/Designer localizer, adapted
//  to run standalone because SpeedTree exposes no Python plug-in API:
//
//  Dictionaries  sp-translation-v1 JSON in <plug-in>/translations/*_zh.json,
//                loaded sorted & merged (later files win).
//  Lookup        1. global exact  2. fuzzy match.
//  DisplayLayer  QProxyStyle (drawItemText / itemTextRect) for every style-drawn
//                control (buttons, checkboxes, menus, group-box titles, tab
//                bars, combo buttons, headers). Uses only Qt's public virtual
//                API -- no mangled symbols, no hand-written x64 sret ABI -- so
//                it is safe across Qt 5 / Qt 6.
//  ItemViewLayer QStyledItemDelegate::displayText for list/tree/table.
//  PaintLayer    QPainter::drawText IAT hook for QLabel + custom-painted text.
//                All 8 overloads return void, so the hook is ABI-safe; it never
//                double-translates because it skips strings already containing CJK.
//  MeasureLayer  QFontMetrics/QFontMetricsF IAT hooks (incl. QFontMetrics::size
//                patched by import-name and QFontMetrics::boundingRect(int,...)
//                which QLabel uses): layout measures the translated width, so
//                sizeHint / eliding follow the language and UI width matches.
//                This is the whole sizing story -- no setMinimumWidth needed.
//  Data model    Never rewritten: no setText/setTabText/setItemText mutations.
//  Controls      F3 enable/disable; Shift+` dumps untranslated text to a JSON.
//  API           C exports so a future settings UI / updater can version it.
//
//  Delivery      launcher.cpp starts SpeedTree_Modeler.exe SUSPENDED, injects
//                this DLL, then resumes. Nothing on disk is modified.
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <set>
#include <cctype>
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
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include <QtCore/QPointer>
#include <QtCore/QModelIndex>
#include <QtCore/QAbstractItemModel>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtWidgets/QAction>
#else
#include <QtGui/QAction>
#endif
#include <QtGui/QPainter>
#include <QtGui/QColor>
#include <QtGui/QFontMetrics>
#include <QtGui/QTextOption>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QCloseEvent>
#include <QtGui/QKeySequence>
#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QLabel>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
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

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
static bool      g_enabled          = true;
static int       g_toggleKey        = Qt::Key_F3;    // 中/英 toggle hotkey (default F3)
static bool      g_hotkeyDlgOpen    = false;         // while the rebind dialog is up
static QString   g_plugInDir;
static HMODULE   g_selfModule       = nullptr;
static bool      g_hooksInstalled   = false;

static std::unordered_map<std::string, std::string> g_translations;       // global
static std::unordered_map<std::string, std::string> g_foldedTranslations; // fuzzy index
// Doubled, lower-cased dictionary keys sorted for O(log N) elided-text lookups.
static std::vector<std::pair<std::string, std::string>> g_sortedKeys;     // lowercased -> original
static std::unordered_map<std::string, std::string> g_normCache;          // text -> normalized
static constexpr size_t kMaxNormCache = 100000;

static std::set<std::string> g_paintSeen;   // untranslated drawText strings (Shift+` dump)

static QString normalizeForMatch(QString text);
static bool containsCjk(const QString& text);
static void installAuthorLink();
static void repaintAll();
static void refreshLayouts();
static bool replaceImports(HMODULE module, void* original, void* replacement);
static bool replaceImportByName(HMODULE module, const char* name, void* replacement,
                                void** outOriginal);
static void captureUntranslated();

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

static void LoadDictionaries() {
    g_translations.clear();
    g_foldedTranslations.clear();
    g_normCache.clear();
    QDir dir(g_plugInDir + "/translations");
    if (!dir.exists()) return;
    QStringList files = dir.entryList({"*_zh.json"}, QDir::Files, QDir::Name);
    for (const QString& name : files) {
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) continue;
        QJsonParseError error{};
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) continue;
        const QJsonObject map = document.object()
                                    .value(QLatin1String("translations"))
                                    .toObject();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            if (!it.value().isString()) continue;
            const QString value = it.value().toString();
            if (it.key().isEmpty() || value.isEmpty()) continue;
            const std::string key = it.key().toUtf8().toStdString();
            g_translations[key] = value.toUtf8().toStdString();
        }
    }
    // Build the fuzzy index (normalized English key -> Chinese).
    for (const auto& kv : g_translations)
        g_foldedTranslations[normalizeForMatch(QString::fromUtf8(kv.first.c_str()))
                                 .toUtf8().constData()] = kv.second;
    // Build the sorted, lower-cased key table for O(log N) elided-text lookups.
    g_sortedKeys.clear();
    g_sortedKeys.reserve(g_translations.size());
    for (const auto& kv : g_translations) {
        std::string low = kv.first;
        for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        g_sortedKeys.emplace_back(std::move(low), kv.first);
    }
    std::sort(g_sortedKeys.begin(), g_sortedKeys.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
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

// ---------------------------------------------------------------------------
// DisplayLayer: QProxyStyle-based painting + sizing translation.
//
// This covers every control whose text is drawn through QStyle (buttons,
// checkboxes, menu items, group-box titles, tab bars, combo buttons, header
// sections, tool buttons). It uses only Qt's public virtual API: no hard-coded
// mangled symbol names and no hand-written x64 sret ABI model, so it is safe
// across Qt 5 / Qt 6. The widget text is never mutated; translation happens at
// the style boundary. The PaintLayer (drawText IAT) below never double-
// translates because it short-circuits on any string that already contains CJK.
// ---------------------------------------------------------------------------
class ChineseProxyStyle : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;

    void drawItemText(QPainter* painter, const QRect& rect, int flags,
                      const QPalette& pal, bool enabled, const QString& text,
                      QPalette::ColorRole textRole = QPalette::NoRole) const override {
        const QString tr = translated(text);
        const QString draw = tr.isEmpty() ? text : tr;
        // The two attribution menu items keep the default menu look but their
        // label is tinted blue (they used to be rich-text links).
        if (text == QStringLiteral("Bilibili神说要凑数汉化") ||
            text == QStringLiteral("Github仓库")) {
            // The base drawItemText() re-selects the pen from the palette, so a
            // setPen() here would be overridden.  Pass a palette whose ButtonText
            // (the role CE_MenuBarItem uses) is blue instead.
            QPalette p = pal;
            const QColor blue(0x66, 0xAA, 0xFF);   // same #66aaff as the old links
            p.setColor(QPalette::WindowText, blue);
            p.setColor(QPalette::ButtonText, blue);
            QProxyStyle::drawItemText(painter, rect, flags, p, enabled, draw, textRole);
            return;
        }
        QProxyStyle::drawItemText(painter, rect, flags, pal, enabled,
                                  draw, textRole);
    }

    QRect itemTextRect(const QFontMetrics& fm, const QRect& r, int flags,
                       bool enabled, const QString& text) const override {
        QString tr = translated(text);
        return QProxyStyle::itemTextRect(fm, r, flags, enabled,
                                         tr.isEmpty() ? text : tr);
    }

};

static bool g_proxyStyleInstalled = false;

static void installProxyStyle() {
    if (g_proxyStyleInstalled || !QApplication::instance()) return;
    // IMPORTANT: never hand QApplication::style() (Qt's globally-owned style) to
    // QProxyStyle -- the proxy would delete it on destruction, and Qt would too,
    // causing a double free. Wrap a *fresh* instance of the same style instead
    // (QProxyStyle takes ownership of the base it is given).
    const QString key = QApplication::style() ? QApplication::style()->objectName()
                                              : QString();
    QStyle* base = QStyleFactory::create(key.isEmpty() ? QStringLiteral("fusion")
                                                       : key);
    if (!base) base = QStyleFactory::create(QStringLiteral("fusion"));
    QApplication::setStyle(new ChineseProxyStyle(base));
    g_proxyStyleInstalled = true;
}

static std::set<QPointer<QAbstractItemView>> g_delegatedViews;

// ---------------------------------------------------------------------------
// Item-view delegation: list/tree/table items are translated via the delegate.
// Everything else is translated only at QPainter::drawText call time.
// ---------------------------------------------------------------------------
static void applyWidget(QWidget* w);

static void applyTranslations() {
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
    // 原生控件（普通 QLabel）:text() 就是完整原文（省略只发生在绘制阶段，
    // 不修改存值），直接返回即可。不再回退到 toolTip。
    return t;
}

// Flip the 中/英 state. Text is never rewritten, so the change is re-rendered
// through the display layers. The toggle runs synchronously so it takes effect
// the moment the mouse is released. Maximized / fullscreen windows are left
// alone so the resize does not fight the user's own window state.
static void toggleTranslation() {
    g_enabled = !g_enabled;
    // Custom-painted / docked controls cache the text widths and item positions
    // they measured at startup and keep using them after a language switch.
    // Broadcasting a font change makes each re-measure its content for the new
    // language (Qt widgets recompute cached metrics in changeEvent(FontChange)),
    // which is what clicking a node also does -- a plain repaint() only redraws
    // the stale cache and is not enough.
    for (QWidget* w : QApplication::allWidgets()) {
        if (!w) continue;
        QEvent ev(QEvent::FontChange);
        QApplication::sendEvent(w, &ev);
    }
    refreshLayouts();
    // Force the (persistent) top menu bar to re-measure its action rects on a
    // language switch.  QMenuBar caches those rects and only recomputes them when
    // itemsDirty is set; QMenuBar::changeEvent() does exactly that on
    // StyleChange / FontChange / ApplicationFontChange, so the menu items
    // shrink/grow to the measured Chinese / English width immediately.
    for (QWidget* w : QApplication::allWidgets()) {
        if (auto* mb = qobject_cast<QMenuBar*>(w)) {
            QEvent ev(QEvent::StyleChange);
            QApplication::sendEvent(mb, &ev);
        }
    }
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (QWidget* w : tops) {
        if (!w || !w->isVisible()) continue;
        // Force a synchronous re-layout so width follows the language width
        // immediately, instead of waiting for a deferred LayoutRequest.
        // QLayout::activate() re-runs doResize() so the dock/central content
        // sizes follow the measured Chinese / English width right now.  (The
        // menu bar's own action rects are refreshed by the ApplicationFontChange
        // loop above, which triggers its itemsDirty -> calcActionRects.)
        if (QLayout* lay = w->layout()) {
            lay->invalidate();   // clear the "already activated" flag
            lay->activate();     // size children (and the menu bar) right now
        }
        if (w->isMaximized() || w->isFullScreen()) continue;
        w->adjustSize();
    }
    repaintAll();
}

// Only wrap the view's *genuinely default* delegate. A custom delegate (which
// SpeedTree uses for its node / property editors) owns its own paint/editor
// logic; replacing it would silently drop that behavior. Text from custom
// delegates is still translated at render time by the QStyle + QPainter hooks,
// so we only need displayText translation for the stock QStyledItemDelegate.
static bool isDefaultItemDelegate(QAbstractItemView* view) {
    QAbstractItemDelegate* d = view->itemDelegate();
    return d && QString::fromLatin1(d->metaObject()->className()) ==
                    QLatin1String("QStyledItemDelegate");
}

// Install only the display-layer delegate. Standard controls are handled by
// drawText hooks, leaving their text properties and sizeHint inputs untouched.
// Called from the event filter on Show / UpdateRequest / Paint as controls appear.
static void applyWidget(QWidget* w) {
    if (!w || !g_enabled) return;
    if (auto* view = qobject_cast<QAbstractItemView*>(w)) {
        if (g_delegatedViews.count(view) == 0 && isDefaultItemDelegate(view)) {
            auto* d = new TranslateDelegate(view);
            view->setItemDelegate(d);
            g_delegatedViews.insert(view);
        }
    }
}

// ---------------------------------------------------------------------------
// translatable-string test (used by the auto-tour and the paint hooks)
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

// ---------------------------------------------------------------------------
// Manual untranslated-text sniffer (Shift + `): walk the live UI read-only and
// append every English string not yet in the dictionary to the user's Desktop
// as <Desktop>\untranslated_zh.json (sp-translation-v1, empty values).  Each
// press only ADDS entries that aren't already there, never overwrites existing
// ones (so manually filled values survive).  Unlike the old ST_TOUR/ST_TRPROBE
// probes it never mutates the UI and only runs on hotkey.
// ---------------------------------------------------------------------------
static void collectMenuActions(QMenu* menu, std::function<void(const QString&)> add) {
    if (!menu) return;
    for (QAction* a : menu->actions()) {
        if (!a->text().isEmpty()) add(a->text());
        if (a->menu()) collectMenuActions(a->menu(), add);
    }
}

static void captureUntranslated() {
    std::map<std::string, std::string> pending;
    auto add = [&](const QString& s) {
        if (!isTranslatableString(s)) return;
        std::string k = s.trimmed().toUtf8().constData();
        if (k.empty() || g_translations.count(k)) return;
        pending.emplace(k, "");
    };
    const auto widgets = QApplication::allWidgets();
    // Cap how many model cells we read on a single press, so an enormously
    // expanded tree can't stall the GUI thread for seconds.
    int cellBudget = 200000;
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
            if (view->model() && cellBudget > 0) {
                auto* model = view->model();
                int rows = model->rowCount(), cols = model->columnCount();
                if (rows > 0 && rows < 100000 && cols > 0 && cols < 64) {
                    for (int r = 0; r < rows && cellBudget > 0; ++r)
                        for (int c = 0; c < cols && cellBudget > 0; ++c) {
                            add(model->data(model->index(r, c), Qt::DisplayRole).toString());
                            --cellBudget;
                        }
                }
            }
        }
    }
    // drawText / custom-painted strings captured since launch.
    for (const std::string& s : g_paintSeen)
        if (!s.empty() && !g_translations.count(s))
            pending.emplace(s, "");

    // Save to the user's Desktop so it is easy to find.
    wchar_t desktop[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktop)) ||
        !desktop[0])
        return;
    const std::wstring path = std::wstring(desktop) + L"\\untranslated_zh.json";

    // Load whatever was captured on a previous press so we only ADD new keys
    // and never clobber existing ones (including any values already filled in).
    std::map<std::string, std::string> merged;
    {
        QFile f(QString::fromStdWString(path));
        if (f.open(QIODevice::ReadOnly)) {
            QJsonParseError err{};
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                const QJsonObject tr = doc.object()
                                           .value(QLatin1String("translations"))
                                           .toObject();
                for (auto it = tr.constBegin(); it != tr.constEnd(); ++it)
                    merged[it.key().toStdString()] = it.value().toString().toStdString();
            }
        }
    }
    int added = 0;
    for (const auto& kv : pending) {
        if (merged.emplace(kv.first, "").second) ++added;
    }
    // 有新增才写文件（避免无意义改写）；无论新增多少都弹窗反馈。
    if (added > 0) {
        std::string json = "{\n  \"$schema\": \"sp-translation-v1\",\n"
                           "  \"id\": \"speedtree-untranslated\",\n"
                           "  \"language\": \"zh-CN\",\n"
                           "  \"description\": \"Shift+` 增量采集的未翻译词条（待填写）\",\n"
                           "  \"translations\": {\n";
        bool first = true;
        for (const auto& kv : merged) {
            if (!first) json += ",\n";
            json += "    \"" + jsonEscape(kv.first) + "\": \"" + jsonEscape(kv.second) + "\"";
            first = false;
        }
        json += "\n  }\n}\n";
        HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD wr = 0;
            const bool wrote = WriteFile(h, json.data(), (DWORD)json.size(), &wr, nullptr) &&
                               wr == json.size();
            CloseHandle(h);
            (void)wrote;
        }
    }
    // Defer the pop-up until after this key-release event is fully processed, so
    // a modal dialog is never shown from inside the event filter.
    const int         addedCount = added;
    const int         totalCount = static_cast<int>(merged.size());
    const std::wstring savedPath = path;
    QTimer::singleShot(0, QApplication::instance(), [addedCount, totalCount, savedPath]() {
        QMessageBox::information(
            nullptr, QStringLiteral("未翻译词条"),
            QStringLiteral("已捕获 ") + QString::number(addedCount) +
                QStringLiteral(" 条（共 ") + QString::number(totalCount) +
                QStringLiteral(" 条）。\n已保存到桌面：\n") +
                QString::fromStdWString(savedPath));
    });
}

class TranslatorFilter : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject* object, QEvent* event) override {
        if (!g_enabled && event->type() != QEvent::KeyRelease)
            return false;
        if (event->type() == QEvent::KeyRelease) {
            auto* ke = static_cast<QKeyEvent*>(event);
            // Ignore OS auto-repeat: a held key emits a burst of repeat events,
            // so without this guard holding F3 (or Shift+`) fires continuously.
            // We act only on the genuine release (isAutoRepeat()==false).
            if (ke->isAutoRepeat()) return false;
            // While the rebind dialog is open, it owns the keyboard to capture a
            // new hotkey.  Never toggle / sniff in that window, or pressing F3
            // to rebind would also flip the language.
            if (g_hotkeyDlgOpen) return false;
            // Shift + ` (the key that also yields ~) dumps untranslated text.
            // The reported key is Key_AsciiTilde (~) on most layouts, or
            // Key_QuoteLeft (`) where the shift doesn't remap it: accept both.
            const bool isTildeKey = ke->key() == Qt::Key_AsciiTilde ||
                                    ke->key() == Qt::Key_QuoteLeft;
            if (isTildeKey && (ke->modifiers() & Qt::ShiftModifier)) {
                captureUntranslated();
                return true;
            }
            if (ke->key() == g_toggleKey) {
                toggleTranslation();
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
typedef void(__fastcall* DrawRectFAlign)(QPainter*, const QRectF&, int, const QString&, QRectF*);
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

static QString translatePaint(QPainter* painter, const QString& text) {
    if (text.isEmpty() || containsCjk(text)) return {};
    // Cheap reject for values that can never be UI text (numbers, tiny tokens),
    // so the expensive QLabel recovery + fuzzy-normalization path is not hit on
    // every draw (the paint hook runs for every drawText in the whole app).
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
    // Recover the FULL source from the painted control (Substance approach):
    // an elided QLabel draws "Mater…" but keeps "Material" in its tooltip /
    // property, so translating the elided fragment misses. Read the complete
    // source from the widget being painted and translate that instead. This is
    // what the delegate does for item views (full DisplayRole) and what the
    // graph/elided-label paths do via the owning property / tooltip.
    if (QWidget* dev = (painter ? dynamic_cast<QWidget*>(painter->device()) : nullptr)) {
        if (auto* lbl = qobject_cast<QLabel*>(dev)) {
            const QString full = labelSource(lbl);
            if (!full.isEmpty() && full != text) {
                const QString fullTr = translated(full);
                if (!fullTr.isEmpty()) return fullTr;
            }
        }
    }
    QString tr = translated(text);
    if (!tr.isEmpty()) return tr;
    // Elided-text recovery: Qt elides by keeping the head and appending "…", so
    // a collapsed label passes "Mater…" to drawText, which never matches the
    // full key "Material". Find the dictionary key that begins with the elided
    // head (prefer the shortest match) so collapsed/truncated labels still
    // translate. Uses the sorted key table for O(log N + matches) instead of a
    // full scan, so it stays cheap on every paint.
    if (text.endsWith(QChar(0x2026)) || text.endsWith(QLatin1String("..."))) {
        QString base = text;
        base.chop(text.endsWith(QChar(0x2026)) ? 1 : 3);
        base = base.trimmed();
        if (base.size() >= 2) {
            std::string low = base.toLower().toUtf8().constData();
            auto it = std::lower_bound(
                g_sortedKeys.begin(), g_sortedKeys.end(), low,
                [](const auto& e, const std::string& v) { return e.first < v; });
            QString best;
            for (; it != g_sortedKeys.end(); ++it) {
                const std::string& k = it->first;
                if (k.compare(0, low.size(), low) != 0) break; // prefix ended
                if (best.isEmpty() ||
                    it->second.size() < static_cast<std::size_t>(best.size()))
                    best = QString::fromUtf8(it->second.c_str());
            }
            if (!best.isEmpty()) {
                const QString fullTr = translated(best);
                if (!fullTr.isEmpty()) return fullTr;
            }
        }
    }
    // Record untranslated paint strings (custom-drawn property labels, etc.)
    // so Shift+` can dump them.  Bounded so a long session never grows without
    // limit.  Re-check the set first so a string that is repainted every frame
    // only pays the expensive isTranslatableString() on its first appearance.
    if (g_paintSeen.size() < 100000) {
        std::string k = text.trimmed().toUtf8().constData();
        if (!k.empty() && g_paintSeen.find(k) == g_paintSeen.end() &&
            isTranslatableString(text))
            g_paintSeen.insert(k);
    }
    return {};
}

static void hookedDrawPoint(QPainter* p, const QPoint& pt, const QString& t) {
    QString tr = translatePaint(p, t);
    if (g_drawPoint) g_drawPoint(p, pt, tr.isEmpty() ? t : tr);
}
static void hookedDrawPointF(QPainter* p, const QPointF& pt, const QString& t) {
    QString tr = translatePaint(p, t);
    if (g_drawPointF) g_drawPointF(p, pt, tr.isEmpty() ? t : tr);
}
static void hookedDrawRect(QPainter* p, const QRect& r, int f, const QString& t, QRect* br) {
    QString tr = translatePaint(p, t);
    if (g_drawRect) g_drawRect(p, r, f, tr.isEmpty() ? t : tr, br);
}
static void hookedDrawRectFOption(QPainter* p, const QRectF& r, const QString& t, const QTextOption& o) {
    QString tr = translatePaint(p, t);
    if (g_drawRectFOption) g_drawRectFOption(p, r, tr.isEmpty() ? t : tr, o);
}
static void hookedDrawRectFAlign(QPainter* p, const QRectF& r, int f, const QString& t, QRectF* br) {
    QString tr = translatePaint(p, t);
    if (g_drawRectFAlign) g_drawRectFAlign(p, r, f, tr.isEmpty() ? t : tr, br);
}
static void hookedDrawXY(QPainter* p, int x, int y, const QString& t) {
    QString tr = translatePaint(p, t);
    if (g_drawXY) g_drawXY(p, x, y, tr.isEmpty() ? t : tr);
}
static void hookedDrawXYWH(QPainter* p, int x, int y, int w, int h, int f, const QString& t, QRect* br) {
    QString tr = translatePaint(p, t);
    if (g_drawXYWH) g_drawXYWH(p, x, y, w, h, f, tr.isEmpty() ? t : tr, br);
}
static void hookedDrawPointF2(QPainter* p, const QPointF& pt, const QString& t, int f, int d) {
    QString tr = translatePaint(p, t);
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
            replaced = true;
        }
    }
    return replaced;
}

// Patch the IAT slot in `module` that imports the symbol `name` so it calls
// `replacement`, and report the ORIGINAL resolved address via *outOriginal.
// Unlike replaceImports(), this matches by the import NAME, not by address.  That
// matters for symbols such as QFontMetrics::size which the loader resolves as a
// normal import (QFontMetrics::size is in Qt6Widgets' IAT pointing into
// Qt6Gui.dll) even though pefile cannot find "size" in Qt6Gui's export table.
// Hooking it is what lets the menu bar / group box / header -- which measure
// text through fm.size() -- re-measure at Chinese width.
static bool replaceImportByName(HMODULE module, const char* name, void* replacement,
                                void** outOriginal) {
    if (outOriginal) *outOriginal = nullptr;
    auto* base = reinterpret_cast<unsigned char*>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    const DWORD importRva = nt->OptionalHeader
        .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!importRva) return false;
    auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + importRva);
    for (; desc->Name; ++desc) {
        if (!desc->OriginalFirstThunk || !desc->FirstThunk) continue;
        auto* oft = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->OriginalFirstThunk);
        auto* ftn = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);
        for (DWORD i = 0; oft[i].u1.AddressOfData; ++i) {
            const ULONGLONG ordinalFlag = 0x8000000000000000ULL;   // IMAGE_ORDINAL_FLAG64
            if (oft[i].u1.AddressOfData & ordinalFlag) continue;    // imported by ordinal
            const char* pname = reinterpret_cast<const char*>(
                base + oft[i].u1.AddressOfData + 2);               // skip 2-byte hint
            if (std::strcmp(pname, name) != 0) continue;
            void** slot = reinterpret_cast<void**>(&ftn[i].u1.Function);
            if (outOriginal) *outOriginal = *slot;
            DWORD old = 0;
            if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
            *slot = replacement;
            DWORD ign = 0;
            VirtualProtect(slot, sizeof(void*), old, &ign);
            FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
            return true;
        }
    }
    return false;
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
// RDX = sret, then the ordinary Qt arguments in registers/stack.  This is the
// MSVC x64 member-function convention and matches SpeedTree's Qt build (which
// is also MSVC x64); a MinGW or 32-bit build would need different modelling,
// so we hard-fail the build on anything other than 64-bit.
static_assert(sizeof(void*) == 8, "SpeedTreeChinese hooks assume MSVC x64 (8-byte pointers)");
typedef void* (__fastcall* FmBRectRaw)(const QFontMetrics* self, QRect* result, const QString& text);
typedef void* (__fastcall* FmBRectOptRaw)(const QFontMetrics* self, QRect* result, const QString& text, const QTextOption& opt);
typedef void* (__fastcall* FmBRectRectRaw)(const QFontMetrics* self, QRect* result, const QRect& rect, int flags, const QString& text, int tabstops, int* tabarray);
typedef void* (__fastcall* FmBRectXYWHRaw)(const QFontMetrics* self, QRect* result, int x, int y, int w, int h, int flags, const QString& text, int tabstops, int* tabarray);
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
static FmBRectXYWHRaw g_fmBRectXYWH = nullptr;
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
// Set only after every resolved QFontMetrics symbol is verified to be a real,
// executable code address.  When false the whole measurement layer is skipped
// (never installs / never calls the raw-ABI pointers) rather than risking a
// crash on a mismatched calling convention.
static bool       g_fontMeasureReady = false;

static QString measureText(const QString& text) {
    if (text.isEmpty() || containsCjk(text)) return {};
    return translated(text);
}

// One CJK glyph is roughly the font's em advance.  Measure a representative "中"
// with the REAL (unhooked) advance so we don't recurse; fall back to the height.
// Adds a one-CJK-character of slack when the text was translated to Chinese, so
// Chinese-leaning UI is a touch roomier and never clips.
static int cjkPad(const QFontMetrics* fm) {
    if (g_fmHAdv) { int w = g_fmHAdv(fm, QStringLiteral("中"), -1); if (w > 0) return w; }
    return fm->height();
}
static int cjkPadF(const QFontMetricsF* fm) {
    if (g_fmfHAdv) { qreal w = g_fmfHAdv(fm, QStringLiteral("中"), -1); if (w > 0) return (int)w; }
    return (int)fm->height();
}

static int hookedFontHAdv(const QFontMetrics* fm, const QString& text, int len) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (len >= 0 && len > t.size()) len = static_cast<int>(t.size());
    if (g_fmHAdv) return g_fmHAdv(fm, t, len) + (tr.isEmpty() ? 0 : cjkPad(fm));
    return 0;
}
static int hookedFontHAdvOpt(const QFontMetrics* fm, const QString& text, const QTextOption& opt) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmHAdvOpt) return g_fmHAdvOpt(fm, t, opt) + (tr.isEmpty() ? 0 : cjkPad(fm));
    return 0;
}
static void* hookedFontBRect(const QFontMetrics* fm, QRect* result, const QString& text) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmBRect) g_fmBRect(fm, result, t);
    if (!tr.isEmpty()) result->setWidth(result->width() + cjkPad(fm));
    return result;
}
static void* hookedFontBRectOpt(const QFontMetrics* fm, QRect* result, const QString& text, const QTextOption& opt) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmBRectOpt) g_fmBRectOpt(fm, result, t, opt);
    if (!tr.isEmpty()) result->setWidth(result->width() + cjkPad(fm));
    return result;
}
static void* hookedFontBRectRect(const QFontMetrics* fm, QRect* result, const QRect& r, int flags, const QString& text, int tabstops, int* tabarray) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmBRectRect) g_fmBRectRect(fm, result, r, flags, t, tabstops, tabarray);
    if (!tr.isEmpty()) result->setWidth(result->width() + cjkPad(fm));
    return result;
}
static void* hookedFontBRectXYWH(const QFontMetrics* fm, QRect* result, int x, int y, int w, int h, int flags, const QString& text, int tabstops, int* tabarray) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmBRectXYWH) g_fmBRectXYWH(fm, result, x, y, w, h, flags, t, tabstops, tabarray);
    if (!tr.isEmpty()) result->setWidth(result->width() + cjkPad(fm));
    return result;
}
static void* hookedFontSize(const QFontMetrics* fm, QSize* result, int flags, const QString& text, int tabstops, int* tabarray) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmSize) g_fmSize(fm, result, flags, t, tabstops, tabarray);
    if (!tr.isEmpty()) result->setWidth(result->width() + cjkPad(fm));
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
    if (g_fmfHAdv) return g_fmfHAdv(fm, t, len) + (tr.isEmpty() ? 0 : cjkPadF(fm));
    return 0;
}
static qreal hookedFontFHAdvOpt(const QFontMetricsF* fm, const QString& text, const QTextOption& opt) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmfHAdvOpt) return g_fmfHAdvOpt(fm, t, opt) + (tr.isEmpty() ? 0 : cjkPadF(fm));
    return 0;
}
static void* hookedFontFBRect(const QFontMetricsF* fm, QRectF* result, const QString& text) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmfBRect) g_fmfBRect(fm, result, t);
    if (!tr.isEmpty()) result->setWidth(result->width() + cjkPadF(fm));
    return result;
}
static void* hookedFontFBRectOpt(const QFontMetricsF* fm, QRectF* result, const QString& text, const QTextOption& opt) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmfBRectOpt) g_fmfBRectOpt(fm, result, t, opt);
    if (!tr.isEmpty()) result->setWidth(result->width() + cjkPadF(fm));
    return result;
}
static void* hookedFontFBRectRect(const QFontMetricsF* fm, QRectF* result, const QRectF& r, int flags, const QString& text, int tabstops, int* tabarray) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmfBRectRect) g_fmfBRectRect(fm, result, r, flags, t, tabstops, tabarray);
    if (!tr.isEmpty()) result->setWidth(result->width() + cjkPadF(fm));
    return result;
}
static void* hookedFontFSize(const QFontMetricsF* fm, QSizeF* result, int flags, const QString& text, int tabstops, int* tabarray) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmfSize) g_fmfSize(fm, result, flags, t, tabstops, tabarray);
    if (!tr.isEmpty()) result->setWidth(result->width() + cjkPadF(fm));
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

// True if p is a committed, executable code address (a real function body, not
// a null / data / freed pointer).  GetProcAddress() normally returns such an
// address; this is a cheap defensive check so the font layer can abandon itself
// if any resolved symbol looks anomalous.
static bool isExecutableCodeAddr(const void* p) {
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS) return false;
    const DWORD exec = PAGE_EXECUTE | PAGE_EXECUTE_READ |
                       PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (mbi.Protect & exec) != 0;
}

// Validate every QFontMetrics/QFontMetricsF symbol that GetProcAddress resolved.
// A null symbol is acceptable (that overload is simply skipped), but a NON-null
// symbol that does not point into executable code means the resolution is bogus,
// so we abandon the whole measurement layer instead of calling into a bad ABI.
static bool fontSymbolsAreValid() {
    const void* ptrs[] = {
        (void*)g_fmHAdv,      (void*)g_fmHAdvOpt,   (void*)g_fmBRect,
        (void*)g_fmBRectOpt,  (void*)g_fmBRectRect, (void*)g_fmBRectXYWH,
        (void*)g_fmSize,      (void*)g_fmTBRect,    (void*)g_fmTBRectOpt,
        (void*)g_fmElide,
        (void*)g_fmfHAdv,     (void*)g_fmfHAdvOpt,  (void*)g_fmfBRect,
        (void*)g_fmfBRectOpt, (void*)g_fmfBRectRect,(void*)g_fmfSize,
        (void*)g_fmfTBRect,   (void*)g_fmfTBRectOpt,(void*)g_fmfElide,
    };
    for (const void* p : ptrs)
        if (p && !isExecutableCodeAddr(p)) return false;
    return true;
}

// (Re)scan every loaded module and redirect the QFontMetrics measurement
// imports (and the QFontMetrics::size import-slot).  Idempotent, so it also
// patches any DLL that loads after the first pass.
static void patchFontModules() {
    if (!g_fontMeasureReady) return;   // layer disabled: never patch/call
    struct Sym { const char* name; void* orig; void* hook; };
    Sym syms[] = {
        {"FMHAdv",      (void*)g_fmHAdv,      (void*)&hookedFontHAdv},
        {"FMHAdvOpt",   (void*)g_fmHAdvOpt,   (void*)&hookedFontHAdvOpt},
        {"FMBRect",     (void*)g_fmBRect,     (void*)&hookedFontBRect},
        {"FMBRectOpt",  (void*)g_fmBRectOpt,  (void*)&hookedFontBRectOpt},
        {"FMBRectRect", (void*)g_fmBRectRect, (void*)&hookedFontBRectRect},
        {"FMBRectXYWH", (void*)g_fmBRectXYWH, (void*)&hookedFontBRectXYWH},
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
                {
                    void* orig = nullptr;
                    if (replaceImportByName(h,
                            "?size@QFontMetrics@@QEBA?AVQSize@@HAEBVQString@@HPEAH@Z",
                            (void*)&hookedFontSize, &orig) && !g_fmSize && orig)
                        g_fmSize = (FmSizeRaw)orig;
                }
                {
                    void* orig = nullptr;
                    if (replaceImportByName(h,
                            "?size@QFontMetricsF@@QEBA?AVQSizeF@@HAEBVQString@@HPEAH@Z",
                            (void*)&hookedFontFSize, &orig) && !g_fmfSize && orig)
                        g_fmfSize = (FmFSizeRaw)orig;
                }
            } while (Module32NextW(snap, &mod));
        }
        CloseHandle(snap);
    }
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
    g_fmBRectXYWH = (FmBRectXYWHRaw)GetProcAddress(qtGui,
        "?boundingRect@QFontMetrics@@QEBA?AVQRect@@HHHHHAEBVQString@@HPEAH@Z");
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

    g_fontHooksInstalled = true;
    // Fail-safe: only turn the measurement layer on if every resolved symbol is
    // a genuine executable address.  On any anomaly, abandon the whole layer --
    // the app degrades to English-width layout instead of risking a raw-ABI
    // call that could corrupt the stack.
    g_fontMeasureReady = fontSymbolsAreValid();
    if (g_fontMeasureReady) patchFontModules();
}

// (Re)scan every loaded module and redirect its QPainter::drawText imports.
// Idempotent: replaceImports() only rewrites slots that still point at the
// ORIGINAL, so re-running patches any DLL that loaded after the first pass.
static void patchDrawTextModules() {
    struct Sym { const char* name; void* orig; void* hook; };
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
}

static void installDrawTextHooks() {
    if (g_hooksInstalled) return;
    HMODULE qtGui = GetModuleHandleW(kGuiModule);
    if (!qtGui) return;
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
    g_hooksInstalled = true;
    patchDrawTextModules();
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
    for (QWidget* w : QApplication::allWidgets()) {
        if (w && w->isVisible()) w->update();
    }
    // The dock / central content can finish re-laying-out on a deferred
    // LayoutRequest *after* this toggle returns, which would leave the panels
    // showing the previous frame.  Repaint again on the next loop iteration so
    // the custom / docked controls redraw at the settled Chinese/English width.
    QTimer::singleShot(0, QApplication::instance(), [] {
        for (QWidget* w : QApplication::allWidgets())
            if (w && w->isVisible()) w->update();
    });
}

static void refreshLayouts() {
    // Prune bookkeeping for views that have been destroyed, so the set stays
    // bounded as dynamic panels come and go.
    for (auto it = g_delegatedViews.begin(); it != g_delegatedViews.end();) {
        if (*it) ++it;
        else it = g_delegatedViews.erase(it);
    }

    for (QWidget* w : QApplication::allWidgets()) {
        // QMenuBar's internal layout uses cached action geometries and does not
        // tolerate a blanket invalidate during startup; invalidating it can make
        // actionGeometry() return garbage and blank the top menu bar. Leave menu
        // bars/menus alone here; toggleTranslation() nudges the menu bar with a
        // StyleChange (which triggers its itemsDirty -> calcActionRects) when the
        // user actually switches language.
        if (qobject_cast<QMenuBar*>(w) || qobject_cast<QMenu*>(w))
            continue;
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
            g_toggleKey = Qt::Key_F3;
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

static QPointer<QAction> g_langAction;
static QPointer<QMenuBar> g_authorLinkBar;   // kept for the hotkey-dialog parent

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
    g_hotkeyDlgOpen = true;
    QObject::connect(dlg, &QObject::destroyed, [] {
        g_hotkeyDlg = nullptr;
        g_hotkeyDlgOpen = false;
    });
    dlg->show(); dlg->raise();
}

class LangMouseFilter : public QObject {
public:
    LangMouseFilter(QObject* p) : QObject(p) {}
    bool eventFilter(QObject* obj, QEvent* ev) override {
        // Qt reports the second press of a double-click as MouseButtonDblClick,
        // not MouseButtonPress. Handle both so a double-click's second release
        // still reaches the toggle below instead of falling through to the
        // menu bar (which caused the "not following the hand" lag).
        if (obj && (ev->type() == QEvent::MouseButtonPress ||
                    ev->type() == QEvent::MouseButtonDblClick) && g_langAction) {
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
    if (g_authorLinkBar) return;      // already installed on this bar
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

    // Author + GitHub as real menu-bar actions (same style as 中/英), each
    // opening its link on click.  Their labels are tinted blue in
    // ChineseProxyStyle::drawItemText.
    QAction* afterLang = nullptr;
    {
        const auto as = mb->actions();
        int li = as.indexOf(langAct);
        afterLang = (li >= 0 && li + 1 < as.size()) ? as[li + 1] : nullptr;
    }
    auto addLink = [&](const QString& label, const QString& url, const char* obj) {
        auto* act = new QAction(label, mb);
        act->setObjectName(QLatin1String(obj));
        act->setData(url);
        QObject::connect(act, &QAction::triggered, [act] {
            const QString u = act->data().toString();
            if (!u.isEmpty()) QDesktopServices::openUrl(QUrl(u));
        });
        if (afterLang) mb->insertAction(afterLang, act);
        else mb->addAction(act);
    };
    addLink(QStringLiteral("Bilibili神说要凑数汉化"), kAuthorUrl, "speedtree_author");
    addLink(QStringLiteral("Github仓库"), kGitHubUrl, "speedtree_github");

    g_authorLinkBar = mb;
}

static void RunOnGuiThread(QApplication* app) {
    LoadDictionaries();
    installProxyStyle();
    applyTranslations();
    installDrawTextHooks();
    installFontMeasurementHooks();
    refreshLayouts();
    app->installEventFilter(new TranslatorFilter(app));
    installAuthorLink();

    // Late-loaded plugin DLLs (e.g. on-demand imports) would otherwise escape the
    // drawText / font-measurement IAT hooks, which only scan modules at startup.
    // Re-scan periodically (idempotent, cheap), but stop after 60 s so we don't
    // burn cycles for the whole session while still catching late-loaded DLLs.
    auto* rescanTimer = new QTimer(app);
    rescanTimer->setInterval(2000);
    QObject::connect(rescanTimer, &QTimer::timeout, [] {
        patchDrawTextModules();
        patchFontModules();
    });
    rescanTimer->start();
    QTimer::singleShot(60000, app, [rescanTimer] { rescanTimer->stop(); });
}

static DWORD WINAPI Worker(LPVOID) {
    QApplication* app = nullptr;
    // Wait up to ~300s for QApplication. SpeedTree can block on licensing /
    // update checks before the GUI is created, so allow a generous window and
    // report a clear diagnostic if we ever give up.
    for (int i = 0; i < 3000; ++i) {
        if (QCoreApplication::instance()) {
            app = qobject_cast<QApplication*>(QCoreApplication::instance());
            if (app) break;
        }
        Sleep(100);
    }
    if (!app) {
        return 1;
    }
    QMetaObject::invokeMethod(app, [app](){ RunOnGuiThread(app); }, Qt::QueuedConnection);
    return 0;
}

// ---------------------------------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        g_selfModule = hinst;
        g_plugInDir = QString::fromStdWString(SelfDirW());
        HANDLE h = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    return TRUE;
}

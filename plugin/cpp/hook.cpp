// ============================================================================
//  SpeedTree Modeler Chinese Localizer - Translation Engine (Hook DLL)
// ============================================================================
//  Architecture mirrors the Substance 3D Painter/Designer localizer, adapted
//  to run standalone because SpeedTree exposes no Python plug-in API:
//
//  Dictionaries  sp-translation-v1 JSON in <plug-in>/translations/*_zh.json,
//                loaded sorted & merged (later files win).
//  Lookup        1. control-ID specific  2. global exact  3. fuzzy match.
//  Display       1. event filter + setText for QLabel/buttons/menus
//                2. QStyledItemDelegate::displayText for list/tree/table
//                3. QPainter::drawText IAT hook for custom-painted controls
//  Controls      F10 enable/disable, F12 write untranslated strings.
//  API           C exports so a future settings UI / updater can version it.
//
//  Delivery      launcher.cpp starts SpeedTree_Modeler.exe SUSPENDED, injects
//                this DLL, then resumes. Nothing on disk is modified.
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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
#include <cstdio>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QLocale>
#include <QtCore/QTimer>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QPointer>
#include <QtCore/QModelIndex>
#include <QtCore/QAbstractItemModel>
#include <QtGui/QAction>
#include <QtGui/QPainter>
#include <QtGui/QTextOption>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QCloseEvent>
#include <QtGui/QKeySequence>
#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QLabel>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QAbstractSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QWidgetAction>
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
static int       g_toggleKey        = Qt::Key_F10;   // 中/英 toggle hotkey
static QString   g_plugInDir;
static HMODULE   g_selfModule       = nullptr;
static bool      g_uiInstalled      = false;
static bool      g_hooksInstalled   = false;

static std::unordered_map<std::string, std::string> g_translations;       // global
static std::unordered_map<std::string, std::string> g_idTranslations;     // control-ID
static std::unordered_map<std::string, std::string> g_foldedTranslations; // fuzzy index
static std::vector<std::pair<void**, void*>>        g_hookSlots;          // IAT slots (slot, replacement)

static std::atomic<long long> g_drawCalls{0};
static std::atomic<long long> g_drawTranslated{0};
static std::set<std::string> g_paintSeen;          // untranslated drawText strings

static QString normalizeForMatch(QString text);
static bool ContainsCjk(const QString& text);
static void installAuthorLink();
static void repaintAll();

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
    return stripped.normalized(QString::NormalizationForm_C);
}

static bool ContainsCjk(const QString& text) {
    for (QChar ch : text) {
        const ushort u = ch.unicode();
        if ((u >= 0x3400 && u <= 0x9FFF) || (u >= 0xF900 && u <= 0xFAFF))
            return true;
    }
    return false;
}

// "Parent||Self||objectName||text" control-ID strings use "||".
static QString translated(const QString& raw, const QString& controlId = QString()) {
    if (!g_enabled) return {};
    QString key = raw.trimmed();
    if (key.isEmpty()) return {};

    if (!controlId.isEmpty()) {
        auto id = g_idTranslations.find(controlId.toUtf8().constData());
        if (id != g_idTranslations.end())
            return id->second == "_skip_" ? QString() :
                   QString::fromUtf8(id->second.c_str());
    }
    auto exact = g_translations.find(key.toUtf8().constData());
    if (exact != g_translations.end())
        return QString::fromUtf8(exact->second.c_str());
    if (ContainsCjk(key)) return {};

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
        if (original.isEmpty() || ContainsCjk(original)) return original;
        QString t = translated(original);
        return t.isEmpty() ? original : t;
    }
};

static std::vector<QPointer<QStyledItemDelegate>> g_delegates;
static std::set<QPointer<QAbstractItemView>> g_delegatedViews;

// ---------------------------------------------------------------------------
// Item-view delegation: list/tree/table items are translated via the model
// delegate. Everything else (labels, buttons, menus, custom-painted text) is
// handled by the QPainter::drawText IAT hook which runs the moment text is drawn.
// ---------------------------------------------------------------------------
static void applyWidget(QWidget* w);

static void applyTranslations(QApplication* app) {
    if (!g_enabled) return;
    for (QWidget* w : QApplication::allWidgets())
        applyWidget(w);
}

// Install the display-layer delegate on item views (list/tree/table). Called
// from the event filter on Show / UpdateRequest / Paint as views appear.
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
static bool IsTranslatableString(const QString& s) {
    QString t = s.trimmed();
    if (t.isEmpty() || ContainsCjk(t)) return false;
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

static void SnifferWrite() {
    std::map<std::string, std::string> pending;
    const auto widgets = QApplication::allWidgets();
    auto add = [&](const QString& s) {
        if (!IsTranslatableString(s)) return;
        std::string k = s.trimmed().toUtf8().constData();
        if (k.empty()) return;
        if (g_translations.count(k) || g_idTranslations.count(k)) return;
        if (pending.count(k)) return;
        pending[k] = "";
    };
    for (QWidget* w : widgets) {
        if (auto* lbl = qobject_cast<QLabel*>(w)) add(lbl->text());
        if (auto* btn = qobject_cast<QAbstractButton*>(w)) add(btn->text());
        if (auto* le = qobject_cast<QLineEdit*>(w)) add(le->text());
        if (auto* cb = qobject_cast<QComboBox*>(w)) {
            add(cb->currentText());
            for (int i = 0; i < cb->count(); ++i) add(cb->itemText(i));
        }
        if (auto* menu = qobject_cast<QMenu*>(w))
            for (QAction* a : menu->actions()) add(a->text());
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
    std::string json = "{\n  \"$schema\": \"sp-translation-v1\",\n"
                       "  \"id\": \"speedtree-sniffed\",\n"
                       "  \"language\": \"zh-CN\",\n"
                       "  \"description\": \"未翻译词条（F12 采集，待填写）\",\n"
                       "  \"translations\": {\n";
    bool first = true;
    for (const auto& kv : pending) {
        if (!first) json += ",\n";
        std::string key = kv.first, val = "";
        // minimal JSON escaping
        std::string ekey; for (char c : key) { if (c == '"' || c == '\\') ekey += '\\'; ekey += c; }
        json += "    \"" + ekey + "\": \"" + val + "\"";
        first = false;
    }
    json += "\n  }\n}\n";
    std::wstring path = SelfDirW() + L"\\untranslated_zh.json";
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD wr = 0;
        WriteFile(h, json.data(), (DWORD)json.size(), &wr, nullptr);
        CloseHandle(h);
    }
    Log("F12 sniffer wrote " + std::to_string(pending.size()) + " entries");
}

static void WritePaintSeen() {
    if (g_paintSeen.empty()) return;
    std::wstring path = SelfDirW() + L"\\drawtext_captured.txt";
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        std::string body;
        for (const std::string& s : g_paintSeen) { body += s; body += "\n"; }
        DWORD wr = 0;
        WriteFile(h, body.data(), (DWORD)body.size(), &wr, nullptr);
        CloseHandle(h);
    }
}

class TranslatorFilter : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject* object, QEvent* event) override {
        if (!g_enabled && event->type() != QEvent::KeyPress) return false;
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == g_toggleKey) {
                g_enabled = !g_enabled;
                // drawText hook + delegate respect g_enabled, so no manual
                // restore needed: disabling reverts to English on next repaint.
                repaintAll();
                Log("F10 toggle enabled=" + std::to_string(g_enabled ? 1 : 0));
                return true;
            }
            if (ke->key() == Qt::Key_F12) {
                SnifferWrite();
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
    ++g_drawCalls;
    if (text.isEmpty() || ContainsCjk(text)) return {};
    QString tr = translated(text);
    if (!tr.isEmpty()) { ++g_drawTranslated; return tr; }
    // Record untranslated paint strings (custom-drawn property labels, etc.).
    if (g_paintSeen.size() < 20000 && IsTranslatableString(text)) {
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
            g_hookSlots.push_back({slot, original});
            replaced = true;
        }
    }
    return replaced;
}

static void installDrawTextHooks() {
    if (g_hooksInstalled) return;
    HMODULE qtGui = GetModuleHandleW(L"Qt6Gui.dll");
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
                if (_wcsicmp(mod.szModule, L"SpeedTreeChineseHook.dll") == 0) continue;
                if (_wcsicmp(mod.szModule, L"Qt6Core.dll") == 0) continue;
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
    Log("drawText IAT hooks installed; slots=" + std::to_string(g_hookSlots.size()));
}

static void restoreDrawTextHooks() {
    for (const auto& kv : g_hookSlots) {
        void** slot = kv.first;
        void* original = kv.second;
        DWORD old = 0;
        if (VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) {
            *slot = original;
            DWORD ign = 0;
            VirtualProtect(slot, sizeof(void*), old, &ign);
            FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
        }
    }
    g_hookSlots.clear();
    g_hooksInstalled = false;
}

// ---------------------------------------------------------------------------
// GUI-thread worker: waits for QApplication, then installs the engine.
// ---------------------------------------------------------------------------
static const QString kAuthorUrl = QStringLiteral("https://space.bilibili.com/281243426?spm_id_from=333.1007.0.0");
static const QString kGitHubUrl = QStringLiteral("");   // fill repo URL here

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
        lay->addWidget(capBtn_);
        lay->addWidget(ok);
        QObject::connect(capBtn_, &QPushButton::clicked, this, [this] { startListen(); });
        QObject::connect(ok, &QPushButton::clicked, this, [this] { stopListen(); accept(); });
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
    const int x = hr.right() + 12;
    const int h = g_authorLinkBar->height();
    g_authorLinkBox->setGeometry(x, 2, g_authorLinkBox->sizeHint().width(), h - 4);
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
    g_enabled = !g_enabled;
    repaintAll();
    Log("中/英 toggle -> " + std::to_string(g_enabled ? 1 : 0));
}

static void openHotkeyDialog() {
    QWidget* parent = g_authorLinkBar ? g_authorLinkBar->window() : nullptr;
    auto* dlg = new ToggleHotkeyDialog(parent);
    dlg->show(); dlg->raise();
}

class LangMouseFilter : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (obj && ev->type() == QEvent::MouseButtonPress && g_langAction) {
            auto* me = static_cast<QMouseEvent*>(ev);
            if (me->button() == Qt::RightButton) {
                auto* mb = qobject_cast<QMenuBar*>(obj);
                if (mb && mb->actionGeometry(g_langAction).contains(me->pos())) {
                    openHotkeyDialog();
                    return true;                       // consume right-click on 中/英
                }
            }
        }
        return false;
    }
};

static void installAuthorLink() {
    static QPointer<QMenuBar> seenBar;
    if (g_authorLinkBox && seenBar) return;
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
    QObject::connect(langAct, &QAction::triggered, [] { doToggleLanguage(); });
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
    lay->addWidget(author);
    lay->addWidget(github);

    g_authorLinkBox = box;
    g_authorLinkBar = mb;
    g_authorHelpAction = langAct;          // position author links after 中/英
    mb->installEventFilter(new AuthorLinkResizeFilter(mb));
    QTimer::singleShot(0, mb, [] { layoutAuthorLinkBox(); });
    layoutAuthorLinkBox();
    seenBar = mb;
    Log("中/英 menu action inserted after Help; author links after it");
}

static void RunOnGuiThread(QApplication* app) {
    LoadDictionaries();
    applyTranslations(app);
    installDrawTextHooks();
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

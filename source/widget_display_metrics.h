#pragma once
#include <windows.h>
#include <QtCore/QtGlobal>
#include <QtGui/QFontMetricsF>
#include <QtGui/QTextOption>

// Only Qt Widgets' imports use translated metrics. The host, Qt Gui, plugin
// code and export renderers continue to see original strings and measurements.
// Numeric sizes and UI-local elision are retained for all standard controls.
namespace WidgetDisplayMetrics {
static QString (*lookup)(const QString&) = nullptr;
static DWORD guiThread = 0;
static unsigned patchedSlots = 0;
static bool sizeHooked = false;
typedef int (__fastcall* FmHAdv)(const QFontMetrics*, const QString&, int);
typedef int (__fastcall* FmHAdvOpt)(const QFontMetrics*, const QString&, const QTextOption&);
// MSVC x64 returns QRect/QSize/QString through a hidden sret pointer. The
// function pointer types below model the raw ABI explicitly: RCX = this,
// RDX = sret, then the ordinary Qt arguments in registers/stack.  This is the
// MSVC x64 member-function convention used by the supported host Qt builds;
// a MinGW or 32-bit build would need different modelling,
// so we hard-fail the build on anything other than 64-bit.
static_assert(sizeof(void*) == 8, "Widget display metric hooks require MSVC x64 (8-byte pointers)");
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
    if (!lookup || GetCurrentThreadId() != guiThread || text.isEmpty()) return {};
    return lookup(text);
}

static int hookedFontHAdv(const QFontMetrics* fm, const QString& text, int len) {
    if (!g_fmHAdv) return 0;
    // A partial source span has no reliable character-by-character mapping
    // into Chinese. Translate only full-string measurements.
    const QString tr = (len < 0 || len >= text.size()) ? measureText(text) : QString();
    return tr.isEmpty() ? g_fmHAdv(fm, text, len) : g_fmHAdv(fm, tr, -1);
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
static void* hookedFontBRectXYWH(const QFontMetrics* fm, QRect* result, int x, int y, int w, int h, int flags, const QString& text, int tabstops, int* tabarray) {
    QString t = text;
    QString tr = measureText(text);
    if (!tr.isEmpty()) t = tr;
    if (g_fmBRectXYWH) g_fmBRectXYWH(fm, result, x, y, w, h, flags, t, tabstops, tabarray);
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
    if (!g_fmfHAdv) return 0;
    const QString tr = (len < 0 || len >= text.size()) ? measureText(text) : QString();
    return tr.isEmpty() ? g_fmfHAdv(fm, text, len) : g_fmfHAdv(fm, tr, -1);
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

// True if p is a committed, executable code address (a real function body, not
// a null / data / freed pointer).  GetProcAddress() normally returns such an
// address; this is a cheap defensive check so the font layer can abandon itself
// if any resolved symbol looks anomalous.
static bool isExecutableCodeAddr(const void* p) {
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT ||
        (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
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

static void patchFontModules() {
    HMODULE module = GetModuleHandleW(QT_VERSION_MAJOR == 6 ? L"Qt6Widgets.dll" : L"Qt5Widgets.dll");
    if (!module) return;
    struct Symbol { void* original; void* replacement; };
    const Symbol symbols[] = {
            {(void*)g_fmHAdv, (void*)&hookedFontHAdv},
            {(void*)g_fmHAdvOpt, (void*)&hookedFontHAdvOpt},
            {(void*)g_fmBRect, (void*)&hookedFontBRect},
            {(void*)g_fmBRectOpt, (void*)&hookedFontBRectOpt},
            {(void*)g_fmBRectRect, (void*)&hookedFontBRectRect},
            {(void*)g_fmBRectXYWH, (void*)&hookedFontBRectXYWH},
            {(void*)g_fmSize, (void*)&hookedFontSize},
            {(void*)g_fmTBRect, (void*)&hookedFontTBRect},
            {(void*)g_fmTBRectOpt, (void*)&hookedFontTBRectOpt},
            {(void*)g_fmElide, (void*)&hookedFontElide},
            {(void*)g_fmfHAdv, (void*)&hookedFontFHAdv},
            {(void*)g_fmfHAdvOpt, (void*)&hookedFontFHAdvOpt},
            {(void*)g_fmfBRect, (void*)&hookedFontFBRect},
            {(void*)g_fmfBRectOpt, (void*)&hookedFontFBRectOpt},
            {(void*)g_fmfBRectRect, (void*)&hookedFontFBRectRect},
            {(void*)g_fmfSize, (void*)&hookedFontFSize},
            {(void*)g_fmfTBRect, (void*)&hookedFontFTBRect},
            {(void*)g_fmfTBRectOpt, (void*)&hookedFontFTBRectOpt},
            {(void*)g_fmfElide, (void*)&hookedFontFElide},
    };
    auto* base = reinterpret_cast<unsigned char*>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;
    const DWORD rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!rva) return;
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + rva);
    for (; descriptor->Name; ++descriptor) {
        if (!descriptor->FirstThunk) continue;
        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);
        for (; thunk->u1.Function; ++thunk) {
            auto** slot = reinterpret_cast<void**>(&thunk->u1.Function);
            for (const auto& symbol : symbols) {
                if (!symbol.original || *slot != symbol.original) continue;
                DWORD oldProtection = 0;
                if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &oldProtection)) continue;
                const bool replaced = InterlockedCompareExchangePointer(
                    reinterpret_cast<PVOID volatile*>(slot), symbol.replacement, symbol.original) == symbol.original;
                DWORD ignored = 0;
                VirtualProtect(slot, sizeof(void*), oldProtection, &ignored);
                if (replaced) {
                    ++patchedSlots;
                    if (symbol.original == reinterpret_cast<void*>(g_fmSize)) sizeHooked = true;
                }
                break;
            }
        }
    }
}

static void install(QString (*translate)(const QString&)) {
    lookup = translate;
    guiThread = GetCurrentThreadId();
    if (g_fontHooksInstalled) return;
    HMODULE qtGui = GetModuleHandleW(QT_VERSION_MAJOR == 6 ? L"Qt6Gui.dll" : L"Qt5Gui.dll");
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

} // namespace WidgetDisplayMetrics

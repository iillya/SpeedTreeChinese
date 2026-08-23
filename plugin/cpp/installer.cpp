// ============================================================================
//  SpeedTree Modeler Chinese Localizer - Self-contained installer (GUI EXE)
// ============================================================================
//  Mirrors the Toolbag localizer installer:
//    * elevated (requireAdministrator manifest)
//    * user picks the SpeedTree install dir (win64 folder with SpeedTree_Modeler.exe)
//    * extracts an embedded payload, installs the hook + launcher + dictionary
//    * creates a "SpeedTree 中文版" desktop / start-menu shortcut
//    * registers the .spm association to open through the launcher
//    * uninstall removes the files, shortcut and restores the original .spm binding
//
//  Embedded payload format (appended after the PE):
//    [ file data... ][ entries... ][ manifestSize u32 ][ count u32 ][ magic u64 ]
//    entry: [ nameLen u32 ][ name bytes ][ offset u64 ][ size u64 ][ sha256 32 ]
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <objbase.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <io.h>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

constexpr uint64_t kMagic = 0x314D4B545343ull; // "SCSTM1"
const wchar_t* kAppTitle = L"SpeedTree 中文补丁安装程序";
constexpr int kInstallBtn = 1001;
constexpr int kUninstallBtn = 1002;
constexpr int kDirEdit = 1003;
constexpr int kBrowseBtn = 1004;

struct PayloadEntry {
    std::wstring name;
    uint64_t offset;
    uint64_t size;
    BYTE sha256[32];
};

struct InstallerState {
    std::wstring dir;
    std::vector<PayloadEntry> entries;
    HWND dirEdit;
    HWND status;
};

int ScaleDpi(int value, UINT dpi) { return MulDiv(value, (int)dpi, 96); }

std::wstring ExePath() {
    std::vector<wchar_t> buf(1024);
    while (buf.size() <= 32768) {
        DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        if (!n) return L"";
        if (n < buf.size() - 1) return std::wstring(buf.data(), n);
        buf.resize(buf.size() * 2);
    }
    return L"";
}

std::wstring ParentDir(const std::wstring& p) {
    size_t s = p.find_last_of(L"\\/");
    return s == std::wstring::npos ? L"" : p.substr(0, s);
}

bool IsDir(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool IsFile(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool HasSpeedTreeExe(const std::wstring& dir) {
    return IsFile(dir + L"\\SpeedTree_Modeler.exe");
}

bool CreateShortcut(const std::wstring& lnkPath, const std::wstring& target,
                    const std::wstring& workDir) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IShellLinkW* link = nullptr;
    if (!SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_IShellLinkW, (void**)&link))) return false;
    IPersistFile* file = nullptr;
    bool ok = false;
    if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, (void**)&file))) {
        link->SetPath(target.c_str());
        link->SetWorkingDirectory(workDir.c_str());
        link->SetDescription(L"SpeedTree 中文版");
        if (SUCCEEDED(file->Save(lnkPath.c_str(), TRUE))) ok = true;
        file->Release();
    }
    link->Release();
    CoUninitialize();
    return ok;
}

// Back up and register the .spm association (HKCU, so no machine-wide risk).
bool SetSpmAssociation(const std::wstring& launcher) {
    HKEY hk = nullptr;
    const wchar_t* progId = L"SpeedTreeChinese.spm";
    std::wstring openCmdStr = L"\"" + launcher + L"\" \"%1\"";
    const wchar_t* openCmd = openCmdStr.c_str();
    // Back up the current default for .spm.
    HKEY bk = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\SpeedTreeChineseLocalizer\\AssocBackup",
                        0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &bk, nullptr) == ERROR_SUCCESS) {
        wchar_t cur[512] = {};
        DWORD cb = sizeof(cur);
        if (RegQueryValueExW(HKEY_CURRENT_USER, L"Software\\Classes\\.spm", nullptr,
                             nullptr, (BYTE*)cur, &cb) == ERROR_SUCCESS)
            RegSetValueExW(bk, L"spm_default", 0, REG_SZ, (const BYTE*)cur, (DWORD)(cb + 1));
        RegCloseKey(bk);
    }
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.spm", 0, nullptr,
                        0, KEY_WRITE, nullptr, &hk, nullptr) != ERROR_SUCCESS) return false;
    RegSetValueExW(hk, nullptr, 0, REG_SZ, (const BYTE*)progId, (DWORD)((wcslen(progId) + 1) * 2));
    RegCloseKey(hk);

    HKEY cmd = nullptr;
    std::wstring cmdPath = std::wstring(L"Software\\Classes\\") + progId + L"\\shell\\open\\command";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, cmdPath.c_str(), 0, nullptr, 0, KEY_WRITE,
                        nullptr, &cmd, nullptr) != ERROR_SUCCESS) return false;
    RegSetValueExW(cmd, nullptr, 0, REG_SZ, (const BYTE*)openCmd, (DWORD)((wcslen(openCmd) + 1) * 2));
    RegCloseKey(cmd);

    wchar_t def[512] = {};
    DWORD dcb = sizeof(def);
    RegQueryValueExW(HKEY_CURRENT_USER, L"Software\\Classes\\SpeedTreeChinese.spm\\shell\\open\\command",
                     nullptr, nullptr, (BYTE*)def, &dcb);
    SHChangeNotify(0x08000000, 0, nullptr, nullptr);
    return true;
}

void RestoreSpmAssociation() {
    HKEY bk = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\SpeedTreeChineseLocalizer\\AssocBackup",
                      0, KEY_READ, &bk) == ERROR_SUCCESS) {
        wchar_t cur[512] = {};
        DWORD cb = sizeof(cur);
        if (RegQueryValueExW(bk, L"spm_default", nullptr, nullptr, (BYTE*)cur, &cb) == ERROR_SUCCESS) {
            HKEY hk = nullptr;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.spm", 0, nullptr,
                                0, KEY_WRITE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
                RegSetValueExW(hk, nullptr, 0, REG_SZ, (const BYTE*)cur, (DWORD)(cb + 1));
                RegCloseKey(hk);
            }
        }
        RegCloseKey(bk);
    }
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\SpeedTreeChinese.spm");
    SHChangeNotify(0x08000000, 0, nullptr, nullptr);
}

// --- embedded payload ------------------------------------------------------
bool ReadWholeFile(const std::wstring& path, std::vector<BYTE>& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{}; if (!GetFileSizeEx(h, &sz)) { CloseHandle(h); return false; }
    if (sz.QuadPart <= 0 || sz.QuadPart > 128ll * 1024 * 1024) { CloseHandle(h); return false; }
    out.resize((size_t)sz.QuadPart);
    DWORD read = 0; BOOL ok = ReadFile(h, out.data(), (DWORD)out.size(), &read, nullptr);
    CloseHandle(h);
    return ok && read == out.size();
}

bool LoadPayload(InstallerState& st) {
    std::vector<BYTE> bytes;
    if (!ReadWholeFile(ExePath(), bytes)) return false;
    if (bytes.size() < 16) return false;
    const size_t tail = bytes.size() - 16;
    uint64_t magic = 0; memcpy(&magic, &bytes[tail], 8);
    if (magic != kMagic) return false;
    uint32_t count = 0; memcpy(&count, &bytes[tail + 8], 4);
    uint32_t manifestSize = 0; memcpy(&manifestSize, &bytes[tail + 12], 4);
    if (count > 512 || (uint64_t)manifestSize > bytes.size()) return false;
    const size_t manifest = tail - manifestSize;
    const char* p = (const char*)&bytes[manifest];
    for (uint32_t i = 0; i < count; ++i) {
        if (p + 4 > (const char*)&bytes[tail]) return false;
        uint32_t nameLen = 0; memcpy(&nameLen, p, 4); p += 4;
        if (nameLen > 512 || p + nameLen > (const char*)&bytes[tail]) return false;
        PayloadEntry e;
        std::string name(p, nameLen); p += nameLen;
        e.name = std::wstring(name.begin(), name.end());
        if (p + 24 > (const char*)&bytes[tail]) return false;
        memcpy(&e.offset, p, 8); p += 8;
        memcpy(&e.size, p, 8); p += 8;
        memcpy(e.sha256, p, 32); p += 32;
        st.entries.push_back(e);
    }
    return true;
}

bool WriteFileFromPayload(const std::wstring& dest, const PayloadEntry& e,
                          const std::vector<BYTE>& payload) {
    if (e.offset + e.size > payload.size() - 16) return false;
    HANDLE h = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    BOOL ok = WriteFile(h, &payload[e.offset], (DWORD)e.size, &wrote, nullptr);
    CloseHandle(h);
    return ok && wrote == e.size;
}

// --- helper: write a status line -------------------------------------------
void SetStatus(InstallerState& st, const wchar_t* text) {
    if (st.status) SetWindowTextW(st.status, text);
}

// --- install -----------------------------------------------------------------
bool Install(InstallerState& st) {
    std::vector<BYTE> payload;
    if (!ReadWholeFile(ExePath(), payload)) { SetStatus(st, L"读取安装包失败"); return false; }
    std::wstring dir = st.dir;
    if (!HasSpeedTreeExe(dir)) { SetStatus(st, L"所选目录不是有效的 SpeedTree 安装目录"); return false; }

    SetStatus(st, L"正在安装…");
    for (const auto& e : st.entries) {
        std::wstring target = dir + L"\\" + e.name;
        std::wstring parent = ParentDir(target);
        if (!IsDir(parent)) CreateDirectoryW(parent.c_str(), nullptr);
        if (!WriteFileFromPayload(target, e, payload)) { SetStatus(st, L"写入失败"); return false; }
    }

    // Create Desktop + Start Menu "SpeedTree 中文版" shortcuts.
    std::wstring launcher = dir + L"\\SpeedTreeChineseLauncher.exe";
    std::wstring name = L"SpeedTree 中文版";
    if (IsFile(launcher)) {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &p)) && p) {
            CreateShortcut(std::wstring(p) + L"\\" + name + L".lnk", launcher, dir);
            CoTaskMemFree(p);
        }
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonStartMenu, 0, nullptr, &p)) && p) {
            CreateShortcut(std::wstring(p) + L"\\" + name + L".lnk", launcher, dir);
            CoTaskMemFree(p);
        }
    }
    SetSpmAssociation(launcher);
    SetStatus(st, L"安装完成（请通过“SpeedTree 中文版”快捷键启动）");
    return true;
}

// --- uninstall ---------------------------------------------------------------
bool Uninstall(InstallerState& st) {
    std::wstring dir = st.dir;
    for (const auto& e : st.entries) {
        std::wstring target = dir + L"\\" + e.name;
        if (IsFile(target)) DeleteFileW(target.c_str());
    }
    // Remove empty translations dir if present.
    std::wstring tr = dir + L"\\translations";
    if (IsDir(tr)) RemoveDirectoryW(tr.c_str());
    // Remove shortcuts.
    PWSTR p = nullptr;
    std::wstring name = L"SpeedTree 中文版.lnk";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &p)) && p) {
        DeleteFileW((std::wstring(p) + L"\\" + name).c_str()); CoTaskMemFree(p);
    }
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonStartMenu, 0, nullptr, &p)) && p) {
        DeleteFileW((std::wstring(p) + L"\\" + name).c_str()); CoTaskMemFree(p);
    }
    RestoreSpmAssociation();
    SetStatus(st, L"已卸载（请删除桌面/开始菜单的“SpeedTree 中文版”快捷键）");
    return true;
}

// --- dialog ------------------------------------------------------------------
INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static InstallerState* st = nullptr;
    switch (msg) {
        case WM_INITDIALOG: {
            st = (InstallerState*)lParam;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            SetWindowTextW(hwnd, kAppTitle);
            st->dirEdit = GetDlgItem(hwnd, kDirEdit);
            st->status = GetDlgItem(hwnd, 1005);
            // Default: the known SpeedTree install path.
            const wchar_t* def = L"C:\\Program Files\\SpeedTree\\SpeedTree Modeler v10.2.0\\win64";
            if (IsDir(def)) { st->dir = def; SetWindowTextW(st->dirEdit, def); }
            if (!LoadPayload(*st)) {
                SetStatus(*st, L"安装包数据不完整（缺少内嵌载荷）");
            }
            return TRUE;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == kBrowseBtn) {
                wchar_t folder[MAX_PATH] = {};
                BROWSEINFOW bi{};
                bi.lpszTitle = L"请选择 SpeedTree 安装目录（含 SpeedTree_Modeler.exe 的 win64 文件夹）";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                PIDLIST_ABSOLUTE pid = SHBrowseForFolderW(&bi);
                if (pid) {
                    if (SHGetPathFromIDListW(pid, folder))
                        SetWindowTextW(st->dirEdit, folder);
                    CoTaskMemFree(pid);
                }
                return TRUE;
            }
            if (id == kInstallBtn || id == kUninstallBtn) {
                wchar_t buf[MAX_PATH] = {};
                GetWindowTextW(st->dirEdit, buf, MAX_PATH);
                st->dir = buf;
                if (id == kInstallBtn) Install(*st);
                else Uninstall(*st);
                return TRUE;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return TRUE;
        case WM_DESTROY:
            PostQuitMessage(0);
            return TRUE;
    }
    return FALSE;
}

} // namespace

int APIENTRY wWinMain(HINSTANCE hinst, HINSTANCE, PWSTR, int nCmdShow) {
    // Elevation is requested via the manifest (requireAdministrator).
    INITCOMMONCONTROLSEX icc{}; icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    InstallerState state{};
    DialogBoxParamW(hinst, MAKEINTRESOURCEW(1), nullptr, DialogProc, (LPARAM)&state);
    CoUninitialize();
    return 0;
}

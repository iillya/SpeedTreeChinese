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
#include <aclapi.h>
#include <sddl.h>
#include <tlhelp32.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <objbase.h>
#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>

#pragma comment(lib, "Comctl32.lib")
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
    HFONT uiFont = nullptr;
    int operationId = 0;
    HWND window = nullptr;
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

// Scan Program Files\SpeedTree for any installed "SpeedTree Modeler vX.Y.Z"
// and return its win64 folder, so the installer defaults to the version that is
// actually present instead of a hard-coded release (works for 9.x and 10.x).
std::wstring FindDefaultSpeedTreeDir() {
    wchar_t pf[MAX_PATH] = {};
    DWORD len = GetEnvironmentVariableW(L"ProgramFiles", pf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        len = GetEnvironmentVariableW(L"ProgramW6432", pf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return L"";
    const std::wstring scanRoot = std::wstring(pf) + L"\\SpeedTree";
    WIN32_FIND_DATAW fd{};
    HANDLE find = FindFirstFileW((scanRoot + L"\\SpeedTree Modeler v*").c_str(), &fd);
    if (find == INVALID_HANDLE_VALUE) return L"";
    std::wstring best;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        const std::wstring base = scanRoot + L"\\" + fd.cFileName;
        for (const wchar_t* sub : {L"win64", L""}) {
            const std::wstring candidate = sub[0] ? (base + L"\\" + sub) : base;
            if (IsFile(candidate + L"\\SpeedTree_Modeler.exe")) {
                best = candidate;
                break;
            }
        }
        if (!best.empty()) break;
    } while (FindNextFileW(find, &fd));
    FindClose(find);
    return best;
}

bool CreateShortcut(const std::wstring& lnkPath, const std::wstring& target,
                    const std::wstring& workDir) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IShellLinkW* link = nullptr;
    if (!SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_IShellLinkW, (void**)&link))) {
        CoUninitialize();
        return false;
    }
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

// The installer runs elevated (requireAdministrator), so the shortcut it writes
// is owned by the elevated account. Grant the target user full control on the
// .lnk so a normal user can always open it; otherwise Windows reports that the
// item referenced by the shortcut cannot be accessed / no permission.
bool GrantShortcutAccess(const std::wstring& lnkPath) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    DWORD required = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &required);
    std::vector<unsigned char> tokenInfo(required);
    const bool tokenRead = required > 0 && GetTokenInformation(
        token, TokenUser, tokenInfo.data(), required, &required) != FALSE;
    CloseHandle(token);
    if (!tokenRead) return false;
    auto* tokenUser = reinterpret_cast<TOKEN_USER*>(tokenInfo.data());

    PSECURITY_DESCRIPTOR securityDescriptor = nullptr;
    PACL currentDacl = nullptr;
    if (GetNamedSecurityInfoW(const_cast<wchar_t*>(lnkPath.c_str()), SE_FILE_OBJECT,
                              DACL_SECURITY_INFORMATION, nullptr, nullptr, &currentDacl,
                              nullptr, &securityDescriptor) != ERROR_SUCCESS) return false;

    EXPLICIT_ACCESSW access{};
    access.grfAccessPermissions = GENERIC_ALL | WRITE_DAC | WRITE_OWNER | DELETE;
    access.grfAccessMode = GRANT_ACCESS;
    access.grfInheritance = NO_INHERITANCE;
    access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    access.Trustee.TrusteeType = TRUSTEE_IS_USER;
    access.Trustee.ptstrName = static_cast<LPWSTR>(tokenUser->User.Sid);

    PACL updatedDacl = nullptr;
    const DWORD aclCreated = SetEntriesInAclW(1, &access, currentDacl, &updatedDacl);
    bool applied = false;
    if (aclCreated == ERROR_SUCCESS) {
        // DACL grant is what makes the shortcut openable; setting the owner is
        // cosmetic and may fail without SeRestorePrivilege, so apply it
        // independently and never let it block the access grant.
        applied = SetNamedSecurityInfoW(
            const_cast<wchar_t*>(lnkPath.c_str()), SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION, nullptr, nullptr, updatedDacl,
            nullptr) == ERROR_SUCCESS;
        SetNamedSecurityInfoW(const_cast<wchar_t*>(lnkPath.c_str()), SE_FILE_OBJECT,
                              OWNER_SECURITY_INFORMATION,
                              static_cast<PSID>(tokenUser->User.Sid),
                              nullptr, nullptr, nullptr);
    }
    if (updatedDacl) LocalFree(updatedDacl);
    LocalFree(securityDescriptor);
    return applied;
}

// The installer runs elevated, so the process identity is an administrator
// account and NOT necessarily the person who will actually run SpeedTree.
// Creating shortcuts against the elevated token puts them in the wrong user's
// Desktop / Start Menu (the user then reports "no shortcut was created").
// Resolve the interactive console session (the explorer.exe owner of the
// current session) and place shortcuts in *that* user's real folders instead.
std::wstring InteractiveSessionSid() {
    DWORD sessionId = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return L"";
    std::wstring found;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"explorer.exe") != 0) continue;
            DWORD processSession = 0;
            if (!ProcessIdToSessionId(entry.th32ProcessID, &processSession) ||
                processSession != sessionId) continue;
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                        FALSE, entry.th32ProcessID);
            if (!process) continue;
            HANDLE token = nullptr;
            if (OpenProcessToken(process, TOKEN_QUERY, &token)) {
                DWORD required = 0;
                GetTokenInformation(token, TokenUser, nullptr, 0, &required);
                std::vector<unsigned char> buffer(required);
                if (required > 0 && GetTokenInformation(
                        token, TokenUser, buffer.data(), required, &required)) {
                    TOKEN_USER* user = reinterpret_cast<TOKEN_USER*>(buffer.data());
                    LPWSTR sidString = nullptr;
                    if (ConvertSidToStringSidW(user->User.Sid, &sidString)) {
                        found = sidString;
                        LocalFree(sidString);
                    }
                }
                CloseHandle(token);
            }
            CloseHandle(process);
            if (!found.empty()) break;
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

// Read the interactive user's real folder path from HKEY_USERS\<SID>, which
// reflects Desktop redirection (e.g. OneDrive) and expands %USERPROFILE%.
std::wstring InteractiveUserShellFolder(const wchar_t* valueName) {
    const std::wstring sid = InteractiveSessionSid();
    if (sid.empty()) return L"";
    HKEY hive = nullptr;
    if (RegOpenKeyExW(HKEY_USERS, sid.c_str(), 0, KEY_READ, &hive) != ERROR_SUCCESS)
        return L"";
    HKEY shellFolders = nullptr;
    if (RegOpenKeyExW(hive,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders",
            0, KEY_READ, &shellFolders) != ERROR_SUCCESS) {
        RegCloseKey(hive);
        return L"";
    }
    wchar_t raw[2048] = {};
    DWORD size = sizeof(raw);
    DWORD type = 0;
    std::wstring value;
    if (RegQueryValueExW(shellFolders, valueName, nullptr, &type,
                         reinterpret_cast<BYTE*>(raw), &size) == ERROR_SUCCESS)
        value = raw;
    RegCloseKey(shellFolders);
    RegCloseKey(hive);
    if (value.empty()) return L"";
    std::vector<wchar_t> expanded(8192);
    const DWORD length = ExpandEnvironmentStringsW(
        value.c_str(), expanded.data(), static_cast<DWORD>(expanded.size()));
    if (length && length <= expanded.size())
        return std::wstring(expanded.data(), length - 1);
    return value;
}

// Back up and register the .spm association (HKCU, so no machine-wide risk).
bool SetSpmAssociation(const std::wstring& launcher) {
    const wchar_t* progId = L"SpeedTreeChinese.spm";
    std::wstring openCmdStr = L"\"" + launcher + L"\" \"%1\"";
    const wchar_t* openCmd = openCmdStr.c_str();
    // Back up whether a user-level .spm default existed and what it was.
    HKEY bk = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\SpeedTreeChineseLocalizer\\AssocBackup",
                        0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &bk, nullptr) == ERROR_SUCCESS) {
        wchar_t cur[512] = {};
        DWORD cb = sizeof(cur);
        if (RegQueryValueExW(HKEY_CURRENT_USER, L"Software\\Classes\\.spm", nullptr,
                             nullptr, (BYTE*)cur, &cb) == ERROR_SUCCESS) {
            // RegQueryValueExW fills cb with the string size INCLUDING the
            // terminating NUL, so write cb bytes (not cb+1, which would read
            // one byte past a full cb-sized buffer and store an extra NUL).
            RegSetValueExW(bk, L"spm_default", 0, REG_SZ, (const BYTE*)cur, (DWORD)cb);
            DWORD one = 1;
            RegSetValueExW(bk, L"spm_had_default", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
        } else {
            DWORD zero = 0;
            RegSetValueExW(bk, L"spm_had_default", 0, REG_DWORD, (const BYTE*)&zero, sizeof(zero));
        }
        RegCloseKey(bk);
    }
    HKEY hk = nullptr;
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

    SHChangeNotify(SHCNE_ASSOCCHANGED, 0, nullptr, nullptr);
    return true;
}

void RestoreSpmAssociation() {
    HKEY bk = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\SpeedTreeChineseLocalizer\\AssocBackup",
                      0, KEY_READ, &bk) == ERROR_SUCCESS) {
        DWORD hadDefault = 0; DWORD hsz = sizeof(hadDefault);
        if (RegQueryValueExW(bk, L"spm_had_default", nullptr, nullptr,
                             (BYTE*)&hadDefault, &hsz) == ERROR_SUCCESS && hadDefault != 0) {
            // A user-level .spm default existed before we installed -> restore it.
            wchar_t cur[512] = {};
            DWORD cb = sizeof(cur);
            if (RegQueryValueExW(bk, L"spm_default", nullptr, nullptr,
                                 (BYTE*)cur, &cb) == ERROR_SUCCESS) {
                HKEY hk = nullptr;
                if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.spm", 0, nullptr,
                                    0, KEY_WRITE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
                    RegSetValueExW(hk, nullptr, 0, REG_SZ, (const BYTE*)cur, (DWORD)cb);
                    RegCloseKey(hk);
                }
            }
        } else {
            // No user-level default before install: drop our override so Windows
            // falls back to the original machine (HKLM) .spm association.
            HKEY hk = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.spm", 0,
                              KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
                RegDeleteValueW(hk, nullptr);
                RegCloseKey(hk);
            }
        }
        RegCloseKey(bk);
    }
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\SpeedTreeChinese.spm");
    // Remove our OpenWithProgids entry for .spm.
    HKEY ow = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.spm\\OpenWithProgids", 0,
                      KEY_SET_VALUE, &ow) == ERROR_SUCCESS) {
        RegDeleteValueW(ow, L"SpeedTreeChinese.spm");
        RegCloseKey(ow);
    }
    // Remove the launcher's "Applications" registration we created.
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\SpeedTreeChineseLauncher.exe");
    SHChangeNotify(SHCNE_ASSOCCHANGED, 0, nullptr, nullptr);
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
    // Tail layout written by embed_files.py: [ manifestSize u32 ][ count u32 ][ magic u64 ]
    uint32_t manifestSize = 0; memcpy(&manifestSize, &bytes[tail], 4);
    uint32_t count = 0;        memcpy(&count,        &bytes[tail + 4], 4);
    uint64_t magic = 0;        memcpy(&magic,        &bytes[tail + 8], 8);
    if (magic != kMagic) return false;
    if (count > 512 || (uint64_t)manifestSize > bytes.size()) return false;
    if (manifestSize > tail) return false;      // guard against offset underflow
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
    // Every payload blob must lie entirely before the manifest/tail, otherwise
    // WriteFileFromPayload would read into (or past) the manifest region.
    for (const auto& e : st.entries) {
        if (e.offset > manifest || e.size > manifest - e.offset) return false;
    }
    return true;
}

bool WriteFileFromPayload(const std::wstring& dest, const PayloadEntry& e,
                          const std::vector<BYTE>& payload) {
    if (e.offset > payload.size() || e.size > payload.size() - e.offset) return false;
    HANDLE h = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    BOOL ok = WriteFile(h, &payload[e.offset], (DWORD)e.size, &wrote, nullptr);
    CloseHandle(h);
    return ok && wrote == e.size;
}

// --- install -----------------------------------------------------------------
bool Install(InstallerState& st) {
    std::vector<BYTE> payload;
    if (!ReadWholeFile(ExePath(), payload)) return false;
    std::wstring dir = st.dir;
    if (!HasSpeedTreeExe(dir)) return false;
    const std::wstring installDir = dir + L"\\ChineseLauncher";
    if (!IsDir(installDir) && !CreateDirectoryW(installDir.c_str(), nullptr))
        return false;

    for (const auto& e : st.entries) {
        std::wstring target = installDir + L"\\" + e.name;
        std::wstring parent = ParentDir(target);
        if (!IsDir(parent)) CreateDirectoryW(parent.c_str(), nullptr);
        if (!WriteFileFromPayload(target, e, payload)) return false;
    }

    // Remove files left by releases that installed directly beside the EXE.
    for (const auto& e : st.entries) {
        const std::wstring legacyTarget = dir + L"\\" + e.name;
        if (IsFile(legacyTarget)) DeleteFileW(legacyTarget.c_str());
    }
    const std::wstring legacyTranslations = dir + L"\\translations";
    if (IsDir(legacyTranslations)) RemoveDirectoryW(legacyTranslations.c_str());

    // Create Desktop + Start Menu "SpeedTree 中文版" shortcuts in the
    // interactive user's real folders (handles elevation + OneDrive redirect).
    std::wstring launcher = installDir + L"\\SpeedTreeChineseLauncher.exe";
    std::wstring name = L"SpeedTree 中文版";
    if (IsFile(launcher)) {
        // Per-user Desktop, resolved from the interactive user's SID.
        std::wstring desktop = InteractiveUserShellFolder(L"Desktop");
        if (desktop.empty()) {
            PWSTR p = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &p)) && p) {
                desktop = p;
                CoTaskMemFree(p);
            }
        }
        if (!desktop.empty()) {
            const std::wstring lnk = desktop + L"\\" + name + L".lnk";
            if (CreateShortcut(lnk, launcher, dir)) GrantShortcutAccess(lnk);
        }
        // Per-user Start Menu (unlike FOLDERID_CommonStartMenu, which is shared
        // and needs admin). Use the interactive user's Programs folder.
        std::wstring programs = InteractiveUserShellFolder(L"Programs");
        if (programs.empty()) {
            PWSTR p = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &p)) && p) {
                programs = p;
                CoTaskMemFree(p);
            }
        }
        if (!programs.empty()) {
            const std::wstring lnk = programs + L"\\" + name + L".lnk";
            if (CreateShortcut(lnk, launcher, dir)) GrantShortcutAccess(lnk);
        }
    }
    SetSpmAssociation(launcher);
    return true;
}

// --- uninstall ---------------------------------------------------------------
bool Uninstall(InstallerState& st) {
    std::wstring dir = st.dir;
    std::error_code removeError;
    std::filesystem::remove_all(
        std::filesystem::path(dir) / L"ChineseLauncher", removeError);
    if (removeError) return false;
    // Clean files left by versions that installed directly beside the EXE.
    for (const auto& e : st.entries) {
        const std::wstring legacyTarget = dir + L"\\" + e.name;
        if (IsFile(legacyTarget)) DeleteFileW(legacyTarget.c_str());
    }
    const std::wstring legacyTranslations = dir + L"\\translations";
    if (IsDir(legacyTranslations)) RemoveDirectoryW(legacyTranslations.c_str());
    // Remove shortcuts from the interactive user's folders (the place the
    // installer actually wrote them to).
    std::wstring name = L"SpeedTree 中文版.lnk";
    std::wstring desktop = InteractiveUserShellFolder(L"Desktop");
    if (desktop.empty()) {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &p)) && p) {
            desktop = p;
            CoTaskMemFree(p);
        }
    }
    std::wstring programs = InteractiveUserShellFolder(L"Programs");
    if (programs.empty()) {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &p)) && p) {
            programs = p;
            CoTaskMemFree(p);
        }
    }
    if (!desktop.empty()) DeleteFileW((desktop + L"\\" + name).c_str());
    if (!programs.empty()) DeleteFileW((programs + L"\\" + name).c_str());
    RestoreSpmAssociation();
    return true;
}

// --- installer window (Toolbag-style, DPI-aware Win32 UI) -------------------
static std::wstring GetControlText(HWND h) {
    std::vector<wchar_t> buf(1024);
    while (buf.size() <= 32768) {
        const int n = GetWindowTextW(h, buf.data(), (int)buf.size());
        if (n < (int)buf.size() - 1) return std::wstring(buf.data(), n);
        buf.resize(buf.size() * 2);
    }
    return L"";
}

static bool SelectSpeedTreeDirectory(std::wstring& dir) {
    wchar_t folder[MAX_PATH] = {};
    BROWSEINFOW bi{};
    bi.lpszTitle = L"请选择 SpeedTree 安装目录（含 SpeedTree_Modeler.exe 的 win64 文件夹）";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pid = SHBrowseForFolderW(&bi);
    if (!pid) return false;
    const bool ok = SHGetPathFromIDListW(pid, folder);
    CoTaskMemFree(pid);
    if (ok) dir = folder;
    return ok;
}

LRESULT CALLBACK InstallerWindowProcedure(HWND window, UINT message,
                                           WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<InstallerState*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = reinterpret_cast<InstallerState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    switch (message) {
    case WM_CREATE: {
        const UINT dpi = GetDpiForWindow(window);
        state->uiFont = CreateFontW(-MulDiv(10, (int)dpi, 72), 0, 0, 0,
            FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
        HWND label = CreateWindowExW(0, L"STATIC", L"SpeedTree 安装目录",
            WS_CHILD | WS_VISIBLE, ScaleDpi(24, dpi), ScaleDpi(20, dpi),
            ScaleDpi(180, dpi), ScaleDpi(22, dpi), window, nullptr, nullptr, nullptr);
        state->dirEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", state->dir.c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            ScaleDpi(24, dpi), ScaleDpi(45, dpi), ScaleDpi(455, dpi), ScaleDpi(31, dpi),
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kDirEdit)),
            nullptr, nullptr);
        HWND browse = CreateWindowExW(0, L"BUTTON", L"浏览…",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            ScaleDpi(489, dpi), ScaleDpi(44, dpi), ScaleDpi(87, dpi), ScaleDpi(33, dpi),
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kBrowseBtn)),
            nullptr, nullptr);
        HWND install = CreateWindowExW(0, L"BUTTON", L"安装汉化",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            ScaleDpi(300, dpi), ScaleDpi(98, dpi), ScaleDpi(130, dpi), ScaleDpi(36, dpi),
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kInstallBtn)),
            nullptr, nullptr);
        HWND uninstall = CreateWindowExW(0, L"BUTTON", L"拆卸汉化",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            ScaleDpi(446, dpi), ScaleDpi(98, dpi), ScaleDpi(130, dpi), ScaleDpi(36, dpi),
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kUninstallBtn)),
            nullptr, nullptr);
        for (HWND c : {label, state->dirEdit, browse, install, uninstall})
            SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(state->uiFont), TRUE);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
        return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }
    case WM_COMMAND: {
        if (!state) return 0;
        const int id = LOWORD(wParam);
        if (id == kBrowseBtn) {
            std::wstring sel = GetControlText(state->dirEdit);
            if (SelectSpeedTreeDirectory(sel))
                SetWindowTextW(state->dirEdit, sel.c_str());
            return 0;
        }
        if (id == kInstallBtn || id == kUninstallBtn) {
            state->dir = GetControlText(state->dirEdit);
            while (!state->dir.empty() &&
                   (state->dir.back() == L' ' || state->dir.back() == L'\\'))
                state->dir.pop_back();
            if (state->dir.empty() || !HasSpeedTreeExe(state->dir)) {
                MessageBoxW(window,
                    L"所选目录不是有效的 SpeedTree 安装目录。\n\n"
                    L"请选择包含 SpeedTree_Modeler.exe 的 win64 目录。",
                    kAppTitle, MB_OK | MB_ICONERROR);
                SetFocus(state->dirEdit);
                return 0;
            }
            state->operationId = id;
            DestroyWindow(window);
            return 0;
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        if (state && state->uiFont) DeleteObject(state->uiFont);
        state->uiFont = nullptr;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

// Scale helper exposed to the window proc (uses the installer's own window dpi).
int ShowInstallerWindow(HINSTANCE instance, InstallerState& state) {
    const wchar_t* cls = L"SpeedTreeChineseInstallerWindow";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = InstallerWindowProcedure;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = cls;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return 0;

    const UINT dpi = GetDpiForSystem();
    RECT frame = {0, 0, ScaleDpi(600, dpi), ScaleDpi(155, dpi)};
    AdjustWindowRectExForDpi(&frame, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                             FALSE, WS_EX_DLGMODALFRAME, dpi);
    const int width = frame.right - frame.left;
    const int height = frame.bottom - frame.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, cls, kAppTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        x, y, width, height, nullptr, nullptr, instance, &state);
    if (!window) return 0;
    state.window = window;
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return state.operationId;
}

} // namespace

int APIENTRY wWinMain(HINSTANCE hinst, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX icc{}; icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    InstallerState state{};
    if (!LoadPayload(state)) {
        MessageBoxW(nullptr, L"安装程序缺少必要数据，请重新下载完整版本。",
                    kAppTitle, MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }
    const std::wstring def = FindDefaultSpeedTreeDir();
    if (!def.empty()) state.dir = def;

    const int operationId = ShowInstallerWindow(hinst, state);
    if (operationId == 0) { CoUninitialize(); return 0; }
    const bool uninstall = operationId == kUninstallBtn;
    const bool ok = uninstall ? Uninstall(state) : Install(state);
    if (ok) {
        MessageBoxW(nullptr,
            uninstall ? L"汉化已拆卸。" :
            L"汉化安装完成。\n\n请通过“SpeedTree 中文版”快捷方式启动，或双击 .spm 文件。",
            kAppTitle, MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(nullptr,
            uninstall ? L"拆卸失败。" : L"安装失败。",
            kAppTitle, MB_OK | MB_ICONERROR);
    }
    CoUninitialize();
    return ok ? 0 : 2;
}

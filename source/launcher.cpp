// ============================================================================
//  SpeedTree Modeler Chinese Localizer - Launcher
// ============================================================================
//  SpeedTree has no plug-in entry point, so this launcher (like the Toolbag
//  localizer) starts SpeedTree_Modeler.exe SUSPENDED, injects the hook DLL via
//  a remote LoadLibraryW thread, then resumes. Original program files are
//  never modified. Any command line (for example a .spm path) is passed
//  through unchanged to SpeedTree.
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>

#include "qt_compatibility.h"

namespace {

std::wstring ParentDir(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"" : path.substr(0, slash);
}

bool IsFile(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

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

std::wstring FindSpeedTreeRoot(const std::wstring& start) {
    std::wstring dir = start;
    for (int depth = 0; depth < 12; ++depth) {
        if (IsFile(dir + L"\\SpeedTree_Modeler.exe") &&
            SpeedTreeQtCompatibility::isAmd64Image(
                dir + L"\\SpeedTree_Modeler.exe") &&
            SpeedTreeQtCompatibility::detectFamily(dir) !=
                SpeedTreeQtCompatibility::Family::none) return dir;
        std::wstring parent = ParentDir(dir);
        if (parent.empty() || parent == dir) break;
        dir = parent;
    }
    // Fall back to scanning Program Files\SpeedTree for any installed version
    // ("SpeedTree Modeler vX.Y.Z\win64"), so the launcher is not tied to one
    // hard-coded release.
    wchar_t pf[MAX_PATH] = {};
    DWORD pfLen = GetEnvironmentVariableW(L"ProgramFiles", pf, MAX_PATH);
    if (pfLen == 0 || pfLen >= MAX_PATH)
        pfLen = GetEnvironmentVariableW(L"ProgramW6432", pf, MAX_PATH);
    if (pfLen && pfLen < MAX_PATH) {
        const std::wstring root = std::wstring(pf) + L"\\SpeedTree";
        WIN32_FIND_DATAW fd{};
        HANDLE h = FindFirstFileW((root + L"\\SpeedTree Modeler v*").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            std::wstring selected;
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                const std::wstring base = root + L"\\" + fd.cFileName;
                for (const wchar_t* sub : {L"win64", L""}) {
                    std::wstring candidate = sub[0] ? (base + L"\\" + sub) : base;
                    if (IsFile(candidate + L"\\SpeedTree_Modeler.exe") &&
                        SpeedTreeQtCompatibility::isAmd64Image(
                            candidate + L"\\SpeedTree_Modeler.exe") &&
                        SpeedTreeQtCompatibility::detectFamily(candidate) !=
                            SpeedTreeQtCompatibility::Family::none) {
                        if (selected.empty() || candidate > selected)
                            selected = candidate;
                    }
                }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
            if (!selected.empty()) return selected;
        }
    }
    return L"";
}

LPTHREAD_START_ROUTINE ResolveLoadLibrary(DWORD pid) {
    HMODULE local = GetModuleHandleW(L"kernel32.dll");
    if (!local) return nullptr;
    FARPROC fn = GetProcAddress(local, "LoadLibraryW");
    if (!fn) return nullptr;
    // A Windows export can be forwarded to KernelBase. Use the module that
    // actually contains its code, not a cross-module offset from kernel32.
    HMODULE owner = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(fn), &owner)) return nullptr;
    wchar_t ownerPath[MAX_PATH] = {};
    const DWORD ownerLength = GetModuleFileNameW(owner, ownerPath, MAX_PATH);
    if (!ownerLength || ownerLength >= MAX_PATH) return nullptr;
    const wchar_t* ownerName = wcsrchr(ownerPath, L'\\');
    ownerName = ownerName ? ownerName + 1 : ownerPath;
    const uintptr_t rva = reinterpret_cast<uintptr_t>(fn) - reinterpret_cast<uintptr_t>(owner);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W mod{}; mod.dwSize = sizeof(mod);
        if (Module32FirstW(snap, &mod)) {
            do {
                if (_wcsicmp(mod.szModule, ownerName) == 0 && rva < mod.modBaseSize) {
                    CloseHandle(snap);
                    return (LPTHREAD_START_ROUTINE)((uintptr_t)mod.modBaseAddr + rva);
                }
            } while (Module32NextW(snap, &mod));
        }
        CloseHandle(snap);
    }
    // A newly created suspended process can temporarily reject module
    // snapshots on some Windows builds. Both processes have the same
    // architecture and share the boot-time system DLL mapping, so retain the
    // proven compatibility fallback used by the stable launcher.
    return reinterpret_cast<LPTHREAD_START_ROUTINE>(fn);
}

} // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR cmdLine, int) {
    const std::wstring own = ExePath();
    const std::wstring dir = ParentDir(own);
    const std::wstring root = FindSpeedTreeRoot(dir);
    const std::wstring exePath = root + L"\\SpeedTree_Modeler.exe";

    // Qt5 builds (SpeedTree 9.x) need the separate Qt5 hook DLL; Qt6 builds
    // (SpeedTree 10.x) use the default Qt6 hook DLL. Detect by the GUI DLL
    // that ships beside the executable.
    const SpeedTreeQtCompatibility::Family family =
        SpeedTreeQtCompatibility::detectFamily(root);
    std::wstring dllPath = dir + L"\\SpeedTreeChineseHook.dll";
    if (family == SpeedTreeQtCompatibility::Family::qt5 &&
        IsFile(dir + L"\\SpeedTreeChineseHook-qt5.dll")) {
        dllPath = dir + L"\\SpeedTreeChineseHook-qt5.dll";
    } else if (family == SpeedTreeQtCompatibility::Family::qt6 &&
               IsFile(dir + L"\\SpeedTreeChineseHook.dll")) {
        dllPath = dir + L"\\SpeedTreeChineseHook.dll";
    } else {
        dllPath.clear();
    }

    if (root.empty() || !IsFile(dllPath) || !IsFile(exePath)) {
        MessageBoxW(nullptr,
                    L"找不到 SpeedTree_Modeler.exe 或汉化 DLL。\n"
                    L"请重新运行安装程序，修复 ChineseLauncher 目录。",
                    L"SpeedTree 中文补丁", MB_OK | MB_ICONERROR);
        return 1;
    }

    STARTUPINFOW si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring cl = L"\"" + exePath + L"\"";
    if (cmdLine && *cmdLine) { cl += L" "; cl += cmdLine; }
    std::vector<wchar_t> clBuf(cl.begin(), cl.end()); clBuf.push_back(L'\0');

    if (!CreateProcessW(exePath.c_str(), clBuf.data(), nullptr, nullptr, FALSE,
                        CREATE_SUSPENDED, nullptr, root.c_str(), &si, &pi)) {
        MessageBoxW(nullptr, L"无法启动 SpeedTree_Modeler.exe。",
                    L"SpeedTree 中文补丁", MB_OK | MB_ICONERROR);
        return 2;
    }

    size_t bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(pi.hProcess, nullptr, bytes,
                                  MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    SIZE_T written = 0;
    bool pathOk = remote &&
        WriteProcessMemory(pi.hProcess, remote, dllPath.c_str(), bytes, &written) &&
        written == bytes;

    auto loadLib = ResolveLoadLibrary(pi.dwProcessId);
    HANDLE thread = nullptr;
    if (pathOk && loadLib)
        thread = CreateRemoteThread(pi.hProcess, nullptr, 0, loadLib, remote, 0, nullptr);

    bool ok = false;
    bool remoteThreadCompleted = false;
    if (thread) {
        DWORD wait = WaitForSingleObject(thread, 30000);
        remoteThreadCompleted = wait == WAIT_OBJECT_0;
        DWORD loadResult = 0;
        const bool loaded = wait == WAIT_OBJECT_0 &&
                            GetExitCodeThread(thread, &loadResult) &&
                            loadResult != 0;
        CloseHandle(thread);
        ok = loaded;
    }

    if (ok && ResumeThread(pi.hThread) != (DWORD)-1) {
        ok = true;
    } else {
        ok = false;
        TerminateProcess(pi.hProcess, 3);
        WaitForSingleObject(pi.hProcess, 5000);
        MessageBoxW(nullptr, L"汉化加载失败，已取消本次 SpeedTree 启动。原程序文件未修改。",
                    L"SpeedTree 中文补丁", MB_OK | MB_ICONERROR);
    }

    // A timed-out remote thread may still be reading the DLL path. The failure
    // path terminates the child, so let process teardown reclaim the memory.
    if (remote && remoteThreadCompleted)
        VirtualFreeEx(pi.hProcess, remote, 0, MEM_RELEASE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return ok ? 0 : 3;
}

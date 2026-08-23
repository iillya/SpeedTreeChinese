// ============================================================================
//  SpeedTree Chinese Localizer - Probe Launcher
// ============================================================================
//  SpeedTree Modeler has no plugin/script entry point, so the probe is
//  delivered exactly like the Toolbag localizer: the launcher starts
//  SpeedTree_Modeler.exe suspended, injects the hook DLL via a remote
//  LoadLibraryW thread, then resumes the main thread. Original files are
//  never modified.
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>

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
        if (IsFile(dir + L"\\SpeedTree_Modeler.exe"))
            return dir;
        std::wstring parent = ParentDir(dir);
        if (parent.empty() || parent == dir) break;
        dir = parent;
    }
    const wchar_t* known =
        L"C:\\Program Files\\SpeedTree\\SpeedTree Modeler v10.2.0\\win64";
    if (IsFile(std::wstring(known) + L"\\SpeedTree_Modeler.exe"))
        return known;
    return L"";
}

LPTHREAD_START_ROUTINE ResolveLoadLibrary(DWORD pid) {
    HMODULE local = GetModuleHandleW(L"kernel32.dll");
    if (!local) return nullptr;
    FARPROC fn = GetProcAddress(local, "LoadLibraryW");
    if (!fn) return nullptr;
    uintptr_t rva = (uintptr_t)fn - (uintptr_t)local;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W mod{};
        mod.dwSize = sizeof(mod);
        if (Module32FirstW(snap, &mod)) {
            do {
                if (_wcsicmp(mod.szModule, L"kernel32.dll") == 0) {
                    CloseHandle(snap);
                    return (LPTHREAD_START_ROUTINE)((uintptr_t)mod.modBaseAddr + rva);
                }
            } while (Module32NextW(snap, &mod));
        }
        CloseHandle(snap);
    }
    return (LPTHREAD_START_ROUTINE)fn;
}

} // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR cmdLine, int) {
    const std::wstring own = ExePath();
    const std::wstring dir = ParentDir(own);
    const std::wstring dllPath = dir + L"\\SpeedTreeChineseHook.dll";
    const std::wstring root = FindSpeedTreeRoot(dir);
    const std::wstring exePath = root + L"\\SpeedTree_Modeler.exe";

    if (root.empty() || !IsFile(dllPath) || !IsFile(exePath)) {
        MessageBoxW(nullptr,
                    L"找不到 SpeedTree_Modeler.exe 或探针 DLL。\n"
                    L"请把 launcher 和 Hook DLL 放在 SpeedTree win64 目录，或在相邻目录运行。",
                    L"SpeedTreeChineseProbe", MB_OK | MB_ICONERROR);
        return 1;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring cl = L"\"" + exePath + L"\"";
    if (cmdLine && *cmdLine) {
        cl += L" ";
        cl += cmdLine;
    }
    std::vector<wchar_t> clBuf(cl.begin(), cl.end());
    clBuf.push_back(L'\0');

    if (!CreateProcessW(exePath.c_str(), clBuf.data(), nullptr, nullptr, FALSE,
                        CREATE_SUSPENDED, nullptr, root.c_str(), &si, &pi)) {
        MessageBoxW(nullptr, L"无法启动 SpeedTree_Modeler.exe。",
                    L"SpeedTreeChineseProbe", MB_OK | MB_ICONERROR);
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
    if (pathOk && loadLib) {
        thread = CreateRemoteThread(pi.hProcess, nullptr, 0, loadLib, remote, 0, nullptr);
    }

    bool ok = false;
    if (thread) {
        DWORD wait = WaitForSingleObject(thread, 30000);
        CloseHandle(thread);
        ok = (wait == WAIT_OBJECT_0);
    }

    if (ok && ResumeThread(pi.hThread) != (DWORD)-1) {
        // Hook installed; main thread resumed. Probe logs when Qt is ready.
        ok = true;
    } else {
        ok = false;
        TerminateProcess(pi.hProcess, 3);
        WaitForSingleObject(pi.hProcess, 5000);
        MessageBoxW(nullptr, L"探针注入失败，已终止 SpeedTree（原文件未受影响）。",
                    L"SpeedTreeChineseProbe", MB_OK | MB_ICONERROR);
    }

    if (remote) VirtualFreeEx(pi.hProcess, remote, 0, MEM_RELEASE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return ok ? 0 : 3;
}

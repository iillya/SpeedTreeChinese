#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <array>
#include <string>
#include <vector>

namespace SpeedTreeQtCompatibility {

enum class Family { none, qt5, qt6 };

inline bool isAmd64Image(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    IMAGE_DOS_HEADER dos{};
    DWORD read = 0;
    bool valid = ReadFile(file, &dos, sizeof(dos), &read, nullptr) &&
                 read == sizeof(dos) && dos.e_magic == IMAGE_DOS_SIGNATURE &&
                 dos.e_lfanew >= static_cast<LONG>(sizeof(dos));
    LARGE_INTEGER offset{};
    offset.QuadPart = valid ? dos.e_lfanew : 0;
    valid = valid && SetFilePointerEx(file, offset, nullptr, FILE_BEGIN);
    DWORD signature = 0;
    IMAGE_FILE_HEADER header{};
    if (valid) {
        valid = ReadFile(file, &signature, sizeof(signature), &read, nullptr) &&
                read == sizeof(signature) && signature == IMAGE_NT_SIGNATURE &&
                ReadFile(file, &header, sizeof(header), &read, nullptr) &&
                read == sizeof(header) &&
                header.Machine == IMAGE_FILE_MACHINE_AMD64;
    }
    CloseHandle(file);
    return valid;
}

inline DWORD fileVersion(const std::wstring& path, unsigned requiredMajor) {
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (!size || size > 1024 * 1024) return 0;
    std::vector<BYTE> bytes(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, bytes.data())) return 0;
    VS_FIXEDFILEINFO* info = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(bytes.data(), L"\\", reinterpret_cast<void**>(&info),
                        &length) ||
        length < sizeof(*info) || info->dwSignature != 0xfeef04bd) {
        return 0;
    }
    const DWORD major = HIWORD(info->dwFileVersionMS);
    const DWORD minor = LOWORD(info->dwFileVersionMS);
    const DWORD patch = HIWORD(info->dwFileVersionLS);
    return major == requiredMajor
        ? (major << 24) | (minor << 12) | patch : 0;
}

inline bool completeFamily(const std::wstring& root, unsigned major) {
    constexpr std::array<const wchar_t*, 3> modules = {
        L"Core.dll", L"Gui.dll", L"Widgets.dll"};
    const std::wstring prefix = root + L"\\Qt" + std::to_wstring(major);
    const DWORD version = fileVersion(prefix + L"Core.dll", major);
    if (!version) return false;
    for (const wchar_t* module : modules) {
        const std::wstring path = prefix + module;
        if (!isAmd64Image(path) || fileVersion(path, major) != version) return false;
    }
    return true;
}

inline Family detectFamily(const std::wstring& root) {
    const bool hasQt5 = GetFileAttributesW((root + L"\\Qt5Core.dll").c_str()) !=
                        INVALID_FILE_ATTRIBUTES;
    const bool hasQt6 = GetFileAttributesW((root + L"\\Qt6Core.dll").c_str()) !=
                        INVALID_FILE_ATTRIBUTES;
    if (hasQt5 == hasQt6) return Family::none;
    if (hasQt5 && completeFamily(root, 5)) return Family::qt5;
    if (hasQt6 && completeFamily(root, 6)) return Family::qt6;
    return Family::none;
}

inline bool supportedRuntime(const char* version, unsigned requiredMajor) noexcept {
    return version && version[0] == static_cast<char>('0' + requiredMajor) &&
           version[1] == '.';
}

} // namespace SpeedTreeQtCompatibility

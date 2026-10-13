#pragma once
// Recover only an association owned by this exact launcher. A complete absence
// of the old journal may recover to a verified same-install official command;
// partial journals and external commands are preserved, never guessed.
namespace ProductLegacy {
using namespace CascadeurProxy;
inline constexpr wchar_t kOldBackup[] = L"Software\\SpeedTreeChineseLocalizer\\AssocBackup";
inline bool exactLauncher(const Value& value, const std::wstring& launcher) {
    std::wstring command;
    return text(value, command) && command == L"\"" + launcher + L"\" \"%1\"";
}
inline bool isLegacyHandler(const std::wstring& name) {
    for (const auto* legacy : kLegacyHandlers) if (_wcsicmp(legacy, name.c_str()) == 0) return true;
    return false;
}
inline bool journalAbsent(HKEY user, std::wstring& error) {
    Key backup;
    const auto status = RegOpenKeyExW(user, kOldBackup, 0, KEY_READ, &backup.value);
    if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) return true;
    error = status == ERROR_SUCCESS ?
        L"旧版工程关联备份存在但不完整或已损坏。请保留备份，不能按备份缺失方式恢复。" :
        L"无法读取旧版工程关联备份的注册表。请登录原安装用户后重试。";
    return false;
}
inline bool officialForExtension(HKEY machine, const std::wstring& root,
                                 const std::wstring& extension, Value& command, std::wstring& error) {
    const auto official = root + L"\\" + hostName(root);
    Value registration;
    std::wstring handler;
    Value proxy;
    if (!regularFile(official) || !amd64(official) ||
        !read(machine, L"Software\\Classes\\" + extension, nullptr, registration) ||
        !text(registration, handler) || handler.empty() || isLegacyHandler(handler) ||
        handler.find(L"..") != std::wstring::npos || handler.find_first_of(L"/:\r\n") != std::wstring::npos ||
        handler.front() == L'\\' ||
        !read(machine, L"Software\\Classes\\" + handler + L"\\shell\\open\\command", nullptr, command) ||
        !makeProxy(command, official, root + L"\\ChineseLauncher\\" + kLauncher, proxy)) {
        error = L"旧版工程关联备份缺失，且官方打开命令与所选安装目录不匹配。请修复原软件的文件关联后重试。";
        return false;
    }
    return true;
}
inline bool importOriginal(HKEY user, HKEY machine, const std::wstring& handler,
                           const std::wstring& launcher, const std::wstring& official,
                           Value& original, std::wstring& error) {
    if (!exactLauncher(original, launcher)) return true;
    const auto root = std::filesystem::path(official).parent_path().wstring();
    if (isLegacyHandler(handler)) {
        // Keep the old ProgID usable when UserChoice still references it.
        original = stringValue(L"\"" + official + L"\" \"%1\"");
        return true;
    }

    if (!journalAbsent(user, error)) return false;
    Value machineCommand, proxy;
    if (!regularFile(official) || !amd64(official) ||
        !read(machine, L"Software\\Classes\\" + handler + L"\\shell\\open\\command", nullptr, machineCommand) ||
        !makeProxy(machineCommand, official, launcher, proxy)) {
        error = L"旧版接管命令没有完整备份，且未找到匹配当前安装目录的官方打开命令。未修改关联。";
        return false;
    }
    original = {}; // Inherit the independently verified official command.
    return true;
}
inline bool ownedLegacy(HKEY user, const std::wstring& root, bool& owned, std::wstring& error) {
    owned = false;
    const auto launcher = root + L"\\ChineseLauncher\\" + kLauncher;
    const auto official = root + L"\\" + hostName(root);
    for (size_t i = 0; i < kLegacyHandlers.size(); ++i) {
        Value command, current;
        std::wstring id, commandText;
        if (!read(user, L"Software\\Classes\\" + std::wstring(kLegacyHandlers[i]) + L"\\shell\\open\\command", nullptr, command) ||
            !read(user, L"Software\\Classes\\" + std::wstring(kExtensions[i]), nullptr, current)) {
            error = L"无法读取旧版工程关联的打开命令。请登录原安装用户后重试。";
            return false;
        }
        if (exactLauncher(command, launcher) ||
            (text(current, id) && _wcsicmp(id.c_str(), kLegacyHandlers[i]) == 0 && text(command, commandText) &&
             commandText == L"\"" + official + L"\" \"%1\"")) owned = true;
    }

    return true;
}
inline bool restoreExtensions(HKEY user, const std::wstring& root, std::wstring& error,
                              bool validateOnly = false, HKEY machine = HKEY_LOCAL_MACHINE) {
    const auto launcher = root + L"\\ChineseLauncher\\" + kLauncher;
    const auto official = root + L"\\" + hostName(root);
    struct Change { std::wstring extension, classPath; Value current, command, original, replacement; bool restoreDefault; };
    std::vector<Change> changes;
    for (size_t i = 0; i < kExtensions.size() && i < kLegacyHandlers.size(); ++i) {
        Change change;
        change.extension = L"Software\\Classes\\" + std::wstring(kExtensions[i]);
        change.classPath = L"Software\\Classes\\" + std::wstring(kLegacyHandlers[i]) + L"\\shell\\open\\command";
        std::wstring id, commandText;
        if (!read(user, change.extension, nullptr, change.current) || !read(user, change.classPath, nullptr, change.command)) {
            error = L"无法读取旧版工程关联的注册表。请登录原安装用户后重试。"; return false;
        }
        const bool launcherOwned = exactLauncher(change.command, launcher);
        if (!launcherOwned && (!text(change.command, commandText) || commandText != L"\"" + official + L"\" \"%1\"")) continue;
        change.restoreDefault = text(change.current, id) && _wcsicmp(id.c_str(), kLegacyHandlers[i]) == 0;
        change.replacement = stringValue(L"\"" + official + L"\" \"%1\"");
        if (change.restoreDefault) {
            const auto prefix = std::wstring(kExtensions[i]).substr(1);
            DWORD had = 0;
            bool saved = dword(user, kOldBackup, (prefix + L"_had_default").c_str(), had) && had <= 1;

            if (saved) {
                std::wstring originalId;
                if (had && (!read(user, kOldBackup, (prefix + L"_default").c_str(), change.original) ||
                    !text(change.original, originalId) || originalId.empty() || _wcsicmp(originalId.c_str(), id.c_str()) == 0)) {
                    error = L"旧版工程关联备份中的原始打开方式无效。未修改关联，请保留备份。"; return false;
                }
            } else {
                if (!journalAbsent(user, error) ||
                    !officialForExtension(machine, root, kExtensions[i], change.replacement, error)) return false;
                change.original = {}; // Recover to verified machine-level inheritance.
            }
        }
        if (launcherOwned) changes.push_back(change);
        else if (change.restoreDefault) changes.push_back(change);
    }
    if (validateOnly) return true;
    // All records are validated before the first mutation. Recheck ownership
    // immediately before writing, preserving subsequent external changes.
    for (const auto& change : changes) {
        Value current, command;
        if (!read(user, change.extension, nullptr, current) || !read(user, change.classPath, nullptr, command)) {
            error = L"无法复核旧版关联，未继续恢复。"; return false;
        }
        if (!(command == change.command)) continue;
        if (change.restoreDefault && current == change.current && !write(user, change.extension, nullptr, change.original)) {
            error = L"无法恢复旧版扩展名关联。请保留安装目录并重试。"; return false;
        }
        if (exactLauncher(command, launcher) && !write(user, change.classPath, nullptr, change.replacement)) {
            error = L"无法把旧版打开命令恢复到原软件。请保留安装目录并重试。"; return false;
        }
    }
    return true;
}

}

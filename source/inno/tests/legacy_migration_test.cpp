#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../support.cpp"
#include <cassert>
#include <iostream>
namespace P = CascadeurProxy;

int main() {
    using namespace P;
    wchar_t exe[32768]{};
    assert(GetModuleFileNameW(nullptr, exe, 32768));
    const auto build = std::filesystem::path(exe).parent_path();
    const auto fixture = build / (L"migration-fixture-" + std::to_wstring(GetCurrentProcessId()));
    assert(!std::filesystem::exists(fixture));
    assert(std::filesystem::create_directory(fixture));
    assert(std::filesystem::copy_file(exe, fixture / L"SpeedTree_Modeler.exe"));
    const auto root = fixture.wstring();
    const auto official = root + L"\\SpeedTree_Modeler.exe";
    const auto launcher = root + L"\\ChineseLauncher\\" + kLauncher;
    const auto extension = L"Software\\Classes\\" + std::wstring(kExtensions[0]);
    const auto classPath = L"Software\\Classes\\" + std::wstring(kLegacyHandlers[0]) + L"\\shell\\open\\command";
    const auto oldCommand = stringValue(L"\"" + launcher + L"\" \"%1\"");
    const auto legacyDefault = stringValue(kLegacyHandlers[0]);
    const auto canonical = stringValue(L"\"" + official + L"\" --open \"%1\" --keep", REG_EXPAND_SZ);
    const std::wstring location = L"Software\\ChineseInstallerMigrationTests\\SPEEDTREE-" + std::to_wstring(GetCurrentProcessId());
    {
        Key sandbox, user, machine;
        assert(RegCreateKeyExW(HKEY_CURRENT_USER, location.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &sandbox.value, nullptr) == ERROR_SUCCESS);
        auto reset = [&] {
            if (user.value) { RegCloseKey(user.value); user.value = nullptr; }
            if (machine.value) { RegCloseKey(machine.value); machine.value = nullptr; }
            RegDeleteTreeW(sandbox.value, L"user");
            RegDeleteTreeW(sandbox.value, L"machine");
            assert(RegCreateKeyExW(sandbox.value, L"user", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &user.value, nullptr) == ERROR_SUCCESS);
            assert(RegCreateKeyExW(sandbox.value, L"machine", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &machine.value, nullptr) == ERROR_SUCCESS);
        };
        auto registerOfficial = [&] {
            assert(write(machine.value, extension, nullptr, stringValue(L"Official.Test.Scene")));
            assert(write(machine.value, L"Software\\Classes\\Official.Test.Scene\\shell\\open\\command", nullptr, canonical));
        };
        auto legacy = [&] {
            assert(write(user.value, extension, nullptr, legacyDefault));
            assert(write(user.value, classPath, nullptr, oldCommand));
        };
        std::wstring error;
        Value value;
        bool owned = true;
        reset();
        assert(ProductLegacy::ownedLegacy(user.value, root, owned, error) && !owned);
        assert(ProductLegacy::restoreExtensions(user.value, root, error, true, machine.value));
        std::cout << "PASS: fresh install without an old journal\n";

        registerOfficial(); legacy();
        assert(ProductLegacy::ownedLegacy(user.value, root, owned, error) && owned);
        assert(ProductLegacy::restoreExtensions(user.value, root, error, true, machine.value));
        assert(read(user.value, extension, nullptr, value) && value == legacyDefault);
        assert(ProductLegacy::restoreExtensions(user.value, root, error, false, machine.value));
        assert(read(user.value, extension, nullptr, value) && !value.present);
        assert(read(user.value, classPath, nullptr, value) && value == canonical);
        assert(read(machine.value, L"Software\\Classes\\Official.Test.Scene\\shell\\open\\command", nullptr, value) && value == canonical);
        std::cout << "PASS: missing journal recovers only to verified official inheritance and preserves arguments/type\n";
        assert(install(user.value, machine.value, root, error));
        assert(uninstall(user.value, root, error));
        assert(read(user.value, extension, nullptr, value) && !value.present);
        assert(read(user.value, classPath, nullptr, value) && value == canonical);
        std::cout << "PASS: recovered association survives new proxy install/uninstall\n";


        reset(); registerOfficial(); legacy();
        const auto backup = std::wstring(ProductLegacy::kOldBackup);
        const auto prefix = std::wstring(kExtensions[0]).substr(1);
        const auto hadName = prefix + L"_had_default";
        const auto defaultName = prefix + L"_default";
        assert(write(user.value, backup, hadName.c_str(), number(1)));

        const auto custom = stringValue(L"Prior.Custom.Handler");
        assert(write(user.value, backup, defaultName.c_str(), custom));
        assert(ProductLegacy::restoreExtensions(user.value, root, error, false, machine.value));
        assert(read(user.value, extension, nullptr, value) && value == custom);
        assert(read(user.value, backup, defaultName.c_str(), value) && value == custom);
        std::cout << "PASS: complete original custom association and journal preserved\n";

        reset(); registerOfficial(); legacy();
        assert(write(user.value, backup, hadName.c_str(), number(2)));
        error.clear();
        assert(!ProductLegacy::restoreExtensions(user.value, root, error, false, machine.value) && !error.empty());
        assert(read(user.value, extension, nullptr, value) && value == legacyDefault);
        assert(read(user.value, classPath, nullptr, value) && value == oldCommand);
        assert(read(user.value, backup, hadName.c_str(), value) && value == number(2));
        std::cout << "PASS: partial/damaged journal rejected without mutation and with a specific reason\n";

        reset(); registerOfficial(); legacy();
        assert(write(machine.value, L"Software\\Classes\\Official.Test.Scene\\shell\\open\\command", nullptr,
            stringValue(L"\"C:\\OtherInstall\\SpeedTree_Modeler.exe\" \"%1\"")));
        error.clear();
        assert(!ProductLegacy::restoreExtensions(user.value, root, error, false, machine.value) && !error.empty());
        assert(read(user.value, extension, nullptr, value) && value == legacyDefault);
        std::cout << "PASS: different installation is not used as recovery evidence\n";

        reset(); registerOfficial();
        const auto external = stringValue(L"\"C:\\Other\\editor.exe\" \"%1\"");
        assert(write(user.value, classPath, nullptr, external));
        assert(write(user.value, extension, nullptr, legacyDefault));
        assert(ProductLegacy::ownedLegacy(user.value, root, owned, error) && !owned);
        assert(ProductLegacy::restoreExtensions(user.value, root, error, false, machine.value));
        assert(read(user.value, classPath, nullptr, value) && value == external);
        assert(read(user.value, extension, nullptr, value) && value == legacyDefault);
        std::cout << "PASS: external replacement preserved\n";

        reset();
        assert(write(user.value, classPath, nullptr, oldCommand));
        assert(ProductLegacy::restoreExtensions(user.value, root, error, false, machine.value));
        assert(read(user.value, classPath, nullptr, value) && value == stringValue(L"\"" + official + L"\" \"%1\""));
        std::cout << "PASS: orphan old ProgID remains usable after launcher removal\n";

        reset();
        assert(write(machine.value, L"Software\\Classes\\Official.Test.Scene\\shell\\open\\command", nullptr, canonical));
        Value original = oldCommand;
        assert(ProductLegacy::importOriginal(user.value, machine.value, L"Official.Test.Scene", launcher, official, original, error));
        assert(!original.present);
        assert(write(user.value, backup, L"Partial", number(1)));
        original = oldCommand;
        assert(!ProductLegacy::importOriginal(user.value, machine.value, L"Official.Test.Scene", launcher, official, original, error));
        assert(original == oldCommand);
        std::cout << "PASS: old direct command with absent journal recovers; partial journal is retained\n";

    }
    assert(RegDeleteTreeW(HKEY_CURRENT_USER, location.c_str()) == ERROR_SUCCESS);
    // The only recursively removed directory is the unique fixture created
    // above, resolved and checked inside this test executable's build folder.
    assert(std::filesystem::canonical(fixture).parent_path() == std::filesystem::canonical(build));
    std::filesystem::remove_all(fixture);
    wchar_t owner[185]{}, message[1024]{};
    assert(LegacyOwnerEx(launcher.c_str(), owner, 185, message, 1024));
    assert(owner[0] == L'\0' && message[0] == L'\0');
    assert(!LegacyOwnerEx(nullptr, owner, 185, message, 1024) && message[0] != L'\0');
    std::cout << "PASS: exported preflight accepts fresh install and returns explicit parameter errors\n";
    DWORD session = 0;
    assert(ProcessIdToSessionId(GetCurrentProcessId(), &session));
    if (session) {
        const auto sid = InstallerUser::sessionSid(session);
        assert(!sid.empty());
        PSID parsed = nullptr;
        assert(ConvertStringSidToSidW(sid.c_str(), &parsed) && IsValidSid(parsed));
        LocalFree(parsed);
        assert(InstallerUser::interactiveSid() == sid);
        std::cout << "PASS: session account resolves without querying explorer.exe or using the elevation account\n";
    }
}

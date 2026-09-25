#pragma once
#include <vector>
#include <string>
inline constexpr wchar_t kProductId[] = L"SpeedTreeChinese";
inline constexpr wchar_t kHostPattern[] = L"SpeedTree_Modeler.exe";
inline constexpr wchar_t kLauncher[] = L"SpeedTreeChineseLauncher.exe";
inline const std::vector<const wchar_t*> kKnownHandlers = {L"SPM File",L"Applications\\SpeedTree_Modeler.exe"};
inline const std::vector<const wchar_t*> kExtensions = {L".spm"};
inline const std::vector<const wchar_t*> kLegacyHandlers = {L"SpeedTreeChinese.spm"};
inline const std::vector<const wchar_t*> kQtModules = {L"Core",L"Gui",L"Widgets"};
inline const std::vector<DWORD> kQtMajors = {5, 6};

#include "Types/GamePlatformVersion.h"
#include "Parsing/GamePlatformCoreAlgorithms.h"

bool FGamePlatformVersion::IsValid() const
{
    return GamePlatformCore::Private::IsValidVersion({Major, Minor, Patch});
}

FString FGamePlatformVersion::ToString() const
{
    const auto Text = GamePlatformCore::Private::FormatVersion<TCHAR>({Major, Minor, Patch});
    return FString(static_cast<int32>(Text.size()), Text.data());
}

bool FGamePlatformVersion::TryParse(const FString& Text, FGamePlatformVersion& OutVersion)
{
    GamePlatformCore::Private::Version Parsed;
    const bool bIsValid = GamePlatformCore::Private::ParseVersion<TCHAR>(
        std::basic_string_view<TCHAR>(*Text, Text.Len()), Parsed);
    OutVersion.Major = Parsed.Major;
    OutVersion.Minor = Parsed.Minor;
    OutVersion.Patch = Parsed.Patch;
    return bIsValid;
}

int32 FGamePlatformVersion::Compare(const FGamePlatformVersion& Other) const
{
    return GamePlatformCore::Private::CompareVersion({Major, Minor, Patch}, {Other.Major, Other.Minor, Other.Patch});
}

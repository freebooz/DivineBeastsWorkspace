#include "Types/GamePlatformId.h"
#include "Parsing/GamePlatformCoreAlgorithms.h"

namespace
{
GamePlatformCore::Private::Identity<TCHAR> CopyIdentity(const FGamePlatformId& Id)
{
    // 保留显式长度，以免内嵌NUL被截断后伪装成合法身份。
    return {std::basic_string<TCHAR>(*Id.Namespace, Id.Namespace.Len()),
        std::basic_string<TCHAR>(*Id.Name, Id.Name.Len()), Id.LogicalVersion};
}
}

bool FGamePlatformId::IsValid() const
{
    return GamePlatformCore::Private::IsValidIdentity(CopyIdentity(*this));
}

FString FGamePlatformId::ToString() const
{
    const auto Text = GamePlatformCore::Private::FormatIdentity(CopyIdentity(*this));
    return FString(static_cast<int32>(Text.size()), Text.data());
}

bool FGamePlatformId::TryParse(const FString& Text, FGamePlatformId& OutId)
{
    GamePlatformCore::Private::Identity<TCHAR> Parsed;
    const bool bIsValid = GamePlatformCore::Private::ParseIdentity<TCHAR>(
        std::basic_string_view<TCHAR>(*Text, Text.Len()), Parsed);
    OutId.Namespace = FString(static_cast<int32>(Parsed.Namespace.size()), Parsed.Namespace.data());
    OutId.Name = FString(static_cast<int32>(Parsed.Name.size()), Parsed.Name.data());
    OutId.LogicalVersion = Parsed.LogicalVersion;
    return bIsValid;
}

bool FGamePlatformId::TryCreate(
    const FString& InNamespace,
    const FString& InName,
    int32 InLogicalVersion,
    FGamePlatformId& OutId)
{
    GamePlatformCore::Private::Identity<TCHAR> Candidate{
        std::basic_string<TCHAR>(*InNamespace, InNamespace.Len()),
        std::basic_string<TCHAR>(*InName, InName.Len()),
        InLogicalVersion};
    if (!GamePlatformCore::Private::IsValidIdentity(Candidate))
    {
        OutId = {};
        return false;
    }

    Candidate.Namespace = GamePlatformCore::Private::CanonicalText<TCHAR>(Candidate.Namespace);
    Candidate.Name = GamePlatformCore::Private::CanonicalText<TCHAR>(Candidate.Name);
    OutId.Namespace = FString(static_cast<int32>(Candidate.Namespace.size()), Candidate.Namespace.data());
    OutId.Name = FString(static_cast<int32>(Candidate.Name.size()), Candidate.Name.data());
    OutId.LogicalVersion = Candidate.LogicalVersion;
    return true;
}

bool FGamePlatformId::operator==(const FGamePlatformId& Other) const
{
    return GamePlatformCore::Private::EqualIdentity(CopyIdentity(*this), CopyIdentity(Other));
}

uint32 GetTypeHash(const FGamePlatformId& Id)
{
    return GamePlatformCore::Private::HashIdentity(CopyIdentity(Id));
}

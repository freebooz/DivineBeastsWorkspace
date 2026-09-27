#include "Types/GamePlatformErrorCode.h"
#include "Parsing/GamePlatformCoreAlgorithms.h"

namespace
{
GamePlatformCore::Private::ErrorCode<TCHAR> CopyErrorCode(const FGamePlatformErrorCode& ErrorCode)
{
    return {
        std::basic_string<TCHAR>(*ErrorCode.Domain, ErrorCode.Domain.Len()),
        std::basic_string<TCHAR>(*ErrorCode.Code, ErrorCode.Code.Len())};
}

void AssignErrorCode(
    const GamePlatformCore::Private::ErrorCode<TCHAR>& Source,
    FGamePlatformErrorCode& OutCode)
{
    OutCode.Domain = FString(static_cast<int32>(Source.Domain.size()), Source.Domain.data());
    OutCode.Code = FString(static_cast<int32>(Source.Code.size()), Source.Code.data());
}
}

bool FGamePlatformErrorCode::IsValid() const
{
    return GamePlatformCore::Private::IsValidErrorCode(CopyErrorCode(*this));
}

FString FGamePlatformErrorCode::ToString() const
{
    const auto Text = GamePlatformCore::Private::FormatErrorCode(CopyErrorCode(*this));
    return FString(static_cast<int32>(Text.size()), Text.data());
}

FName FGamePlatformErrorCode::ToName() const
{
    const FString Text = ToString();
    return Text.IsEmpty() ? NAME_None : FName(*Text);
}

bool FGamePlatformErrorCode::TryParse(const FString& Text, FGamePlatformErrorCode& OutCode)
{
    GamePlatformCore::Private::ErrorCode<TCHAR> Parsed;
    const bool bValid = GamePlatformCore::Private::ParseErrorCode<TCHAR>(
        std::basic_string_view<TCHAR>(*Text, Text.Len()), Parsed);
    AssignErrorCode(Parsed, OutCode);
    return bValid;
}

bool FGamePlatformErrorCode::TryCreate(
    const FString& InDomain,
    const FString& InCode,
    FGamePlatformErrorCode& OutCode)
{
    GamePlatformCore::Private::ErrorCode<TCHAR> Candidate{
        std::basic_string<TCHAR>(*InDomain, InDomain.Len()),
        std::basic_string<TCHAR>(*InCode, InCode.Len())};

    if (!GamePlatformCore::Private::IsValidErrorCode(Candidate))
    {
        OutCode = {};
        return false;
    }

    Candidate.Domain = GamePlatformCore::Private::CanonicalText<TCHAR>(Candidate.Domain);
    Candidate.Code = GamePlatformCore::Private::CanonicalText<TCHAR>(Candidate.Code);
    AssignErrorCode(Candidate, OutCode);
    return true;
}

bool FGamePlatformErrorCode::operator==(const FGamePlatformErrorCode& Other) const
{
    return GamePlatformCore::Private::EqualErrorCode(CopyErrorCode(*this), CopyErrorCode(Other));
}

uint32 GetTypeHash(const FGamePlatformErrorCode& ErrorCode)
{
    return GamePlatformCore::Private::HashErrorCode(CopyErrorCode(ErrorCode));
}

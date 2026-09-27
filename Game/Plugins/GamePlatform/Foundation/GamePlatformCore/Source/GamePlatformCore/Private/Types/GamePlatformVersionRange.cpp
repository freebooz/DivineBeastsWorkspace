#include "Types/GamePlatformVersionRange.h"
#include "Parsing/GamePlatformCoreAlgorithms.h"

namespace
{
GamePlatformCore::Private::Version ToNative(const FGamePlatformVersion& Value)
{
    return {Value.Major, Value.Minor, Value.Patch};
}

GamePlatformCore::Private::VersionRange ToNative(const FGamePlatformVersionRange& Value)
{
    return {Value.bConfigured, ToNative(Value.MinimumInclusive), ToNative(Value.MaximumInclusive)};
}
}

FGamePlatformVersionRange FGamePlatformVersionRange::Inclusive(
    const FGamePlatformVersion& Minimum,
    const FGamePlatformVersion& Maximum)
{
    FGamePlatformVersionRange Range;
    Range.bConfigured = true;
    Range.MinimumInclusive = Minimum;
    Range.MaximumInclusive = Maximum;
    if (!Range.IsValid())
    {
        return {};
    }
    return Range;
}

bool FGamePlatformVersionRange::IsValid() const
{
    return GamePlatformCore::Private::IsValidVersionRange(ToNative(*this));
}

bool FGamePlatformVersionRange::Contains(const FGamePlatformVersion& Candidate) const
{
    return GamePlatformCore::Private::ContainsVersion(ToNative(*this), ToNative(Candidate));
}

FString FGamePlatformVersionRange::ToString() const
{
    if (!IsValid())
    {
        return {};
    }
    return MinimumInclusive.ToString() + TEXT("..") + MaximumInclusive.ToString();
}

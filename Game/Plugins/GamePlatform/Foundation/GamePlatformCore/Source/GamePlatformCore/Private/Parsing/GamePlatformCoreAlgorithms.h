#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// UE适配与原生测试共用的生产算法，保持私有；不持有世界、全局状态或资产。
namespace GamePlatformCore::Private
{
template<typename Character>
struct Identity
{
    std::basic_string<Character> Namespace;
    std::basic_string<Character> Name;
    std::int32_t LogicalVersion = 1;
};

struct Version
{
    std::int32_t Major = 0;
    std::int32_t Minor = 0;
    std::int32_t Patch = 0;
};

template<typename Character>
constexpr Character LowerAscii(Character Value)
{
    return Value >= 'A' && Value <= 'Z' ? static_cast<Character>(Value + ('a' - 'A')) : Value;
}

template<typename Character>
bool IsIdentifier(std::basic_string_view<Character> Text)
{
    if (Text.empty() || Text.size() > 64) { return false; }
    for (std::size_t Index = 0; Index < Text.size(); ++Index)
    {
        const Character Value = LowerAscii(Text[Index]);
        const bool bIsLetter = Value >= 'a' && Value <= 'z';
        const bool bIsSuffix = Index > 0 && ((Value >= '0' && Value <= '9') || Value == '_');
        if (!bIsLetter && !bIsSuffix) { return false; }
    }
    return true;
}

template<typename Character>
bool IsNamespace(std::basic_string_view<Character> Text)
{
    std::size_t Start = 0;
    for (;;)
    {
        const std::size_t End = Text.find(static_cast<Character>('.'), Start);
        const std::size_t Count = End == Text.npos ? Text.size() - Start : End - Start;
        if (!IsIdentifier(Text.substr(Start, Count))) { return false; }
        if (End == Text.npos) { return true; }
        Start = End + 1;
    }
}

/** 严格无符号十进制转int32：乘加之前检测溢出；失败清零，不接受符号和前导零。 */
template<typename Character>
bool ParseNumber(std::basic_string_view<Character> Text, std::int32_t& OutNumber)
{
    OutNumber = 0;
    if (Text.empty() || Text.size() > 10 || (Text.size() > 1 && Text.front() == '0')) { return false; }
    std::int32_t Number = 0;
    for (const Character Value : Text)
    {
        if (Value < '0' || Value > '9') { return false; }
        const std::int32_t Digit = static_cast<std::int32_t>(Value - '0');
        if (Number > (std::numeric_limits<std::int32_t>::max() - Digit) / 10) { return false; }
        Number = Number * 10 + Digit;
    }
    OutNumber = Number;
    return true;
}

template<typename Character>
std::basic_string<Character> Decimal(std::int32_t Value)
{
    // 调用者已经校验非负；逐字符扩展ASCII，避免依赖区域设置与宽字符格式化差异。
    const std::string Ascii = std::to_string(Value);
    return std::basic_string<Character>(Ascii.begin(), Ascii.end());
}

template<typename Character>
bool IsValidIdentity(const Identity<Character>& Value)
{
    // 先限制字段再求总长，避免可编辑超长输入造成size_t加法溢出和无界扫描。
    if (Value.Namespace.size() > 192 || Value.Name.size() > 64 || Value.LogicalVersion < 1) { return false; }
    if (!IsNamespace<Character>(Value.Namespace) || !IsIdentifier<Character>(Value.Name)) { return false; }
    return Value.Namespace.size() + Value.Name.size() + 2 + Decimal<Character>(Value.LogicalVersion).size() <= 192;
}

template<typename Character>
std::basic_string<Character> CanonicalText(std::basic_string_view<Character> Text)
{
    std::basic_string<Character> Result(Text);
    for (Character& Value : Result) { Value = LowerAscii(Value); }
    return Result;
}

template<typename Character>
std::basic_string<Character> FormatIdentity(const Identity<Character>& Value)
{
    if (!IsValidIdentity(Value)) { return {}; }
    return CanonicalText<Character>(Value.Namespace) + static_cast<Character>('.') +
        CanonicalText<Character>(Value.Name) + static_cast<Character>('@') + Decimal<Character>(Value.LogicalVersion);
}

template<typename Character>
bool ParseIdentity(std::basic_string_view<Character> Text, Identity<Character>& OutIdentity)
{
    Identity<Character> Parsed;
    bool bIsValid = false;
    if (Text.size() <= 192)
    {
        const std::size_t At = Text.find(static_cast<Character>('@'));
        const std::size_t Dot = Text.rfind(static_cast<Character>('.'), At);
        if (At != Text.npos && Dot != Text.npos && Dot < At &&
            ParseNumber(Text.substr(At + 1), Parsed.LogicalVersion))
        {
            Parsed.Namespace = CanonicalText<Character>(Text.substr(0, Dot));
            Parsed.Name = CanonicalText<Character>(Text.substr(Dot + 1, At - Dot - 1));
            bIsValid = IsValidIdentity(Parsed);
        }
    }
    // 最后覆盖，允许Text来自OutIdentity字段；失败不留下部分解析或之前的有效身份。
    OutIdentity = bIsValid ? std::move(Parsed) : Identity<Character>{};
    return bIsValid;
}

template<typename Character>
bool EqualIdentity(const Identity<Character>& Left, const Identity<Character>& Right)
{
    const bool bLeftValid = IsValidIdentity(Left);
    const bool bRightValid = IsValidIdentity(Right);
    if (bLeftValid != bRightValid) { return false; }
    if (Left.LogicalVersion != Right.LogicalVersion) { return false; }
    if (!bLeftValid) { return Left.Namespace == Right.Namespace && Left.Name == Right.Name; }
    return CanonicalText<Character>(Left.Namespace) == CanonicalText<Character>(Right.Namespace) &&
        CanonicalText<Character>(Left.Name) == CanonicalText<Character>(Right.Name);
}

template<typename Character>
std::uint32_t HashIdentity(const Identity<Character>& Value)
{
    const bool bIsValid = IsValidIdentity(Value);
    std::uint32_t Hash = 2166136261u;
    const auto Append = [&Hash](std::uint32_t Unit)
    {
        // 固定32位无符号溢出是哈希算法定义的一部分，不执行有符号溢出。
        for (unsigned Shift = 0; Shift < 32; Shift += 8)
        {
            Hash = (Hash ^ ((Unit >> Shift) & 0xffu)) * 16777619u;
        }
    };
    Append(bIsValid ? 1u : 0u);
    for (const Character Unit : Value.Namespace)
    {
        Append(static_cast<std::uint32_t>(static_cast<std::make_unsigned_t<Character>>(bIsValid ? LowerAscii(Unit) : Unit)));
    }
    Append(0xffffffffu);
    for (const Character Unit : Value.Name)
    {
        Append(static_cast<std::uint32_t>(static_cast<std::make_unsigned_t<Character>>(bIsValid ? LowerAscii(Unit) : Unit)));
    }
    Append(0xffffffffu);
    Append(static_cast<std::uint32_t>(Value.LogicalVersion));
    return Hash;
}

inline bool IsValidVersion(const Version& Value)
{
    return Value.Major >= 0 && Value.Minor >= 0 && Value.Patch >= 0;
}

template<typename Character>
std::basic_string<Character> FormatVersion(const Version& Value)
{
    if (!IsValidVersion(Value)) { return {}; }
    return Decimal<Character>(Value.Major) + static_cast<Character>('.') + Decimal<Character>(Value.Minor) +
        static_cast<Character>('.') + Decimal<Character>(Value.Patch);
}

template<typename Character>
bool ParseVersion(std::basic_string_view<Character> Text, Version& OutVersion)
{
    Version Parsed;
    bool bIsValid = false;
    // 三个最多10位的int32分量和两个点；无需在异常超长输入上遍历。
    if (Text.size() <= 32)
    {
        const std::size_t First = Text.find(static_cast<Character>('.'));
        if (First != Text.npos)
        {
            const std::size_t Second = Text.find(static_cast<Character>('.'), First + 1);
            bIsValid = Second != Text.npos && ParseNumber(Text.substr(0, First), Parsed.Major) &&
                ParseNumber(Text.substr(First + 1, Second - First - 1), Parsed.Minor) &&
                ParseNumber(Text.substr(Second + 1), Parsed.Patch);
        }
    }
    OutVersion = bIsValid ? Parsed : Version{};
    return bIsValid;
}

inline std::int32_t CompareVersion(const Version& Left, const Version& Right)
{
    // 不用减法，编辑器尚未校验的负值与INT32_MAX比较也不得溢出。
    if (Left.Major != Right.Major) { return Left.Major < Right.Major ? -1 : 1; }
    if (Left.Minor != Right.Minor) { return Left.Minor < Right.Minor ? -1 : 1; }
    if (Left.Patch != Right.Patch) { return Left.Patch < Right.Patch ? -1 : 1; }
    return 0;
}
}

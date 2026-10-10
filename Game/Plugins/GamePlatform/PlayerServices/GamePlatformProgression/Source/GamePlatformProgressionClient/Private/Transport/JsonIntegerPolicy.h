// 客户端领域JSON整数策略：保留原始数字token，在转换前验证数学整数和int64范围；不改变线协议类型。
// 此纯值策略不访问UObject/网络。各模块各自拥有私有实现，不包含其他模块Private。
#pragma once
#include <string_view>
#include <cstdint>
#include <limits>
namespace GPIntegerPolicy
{
// JSON number允许小数/指数拼写，但值必须为整数；全程十进制运算，避免double在2^63处失真。
template<class Char> bool ParseJsonInteger(std::basic_string_view<Char> Text, std::int64_t& Out)
{
    if (Text.empty() || Text.size() > 1024) { return false; }
    std::size_t Pos = 0; const bool Negative = Text[Pos] == Char('-'); if (Negative) { ++Pos; }
    const auto Digit = [](Char C) { return C >= Char('0') && C <= Char('9'); };
    if (Pos == Text.size() || !Digit(Text[Pos])) { return false; }
    const auto Start = Pos; while (Pos < Text.size() && Digit(Text[Pos])) { ++Pos; }
    if (Pos - Start > 1 && Text[Start] == Char('0')) { return false; }
    Char Digits[1024]; std::size_t Count = 0;
    for (auto I = Start; I < Pos; ++I) { Digits[Count++] = Text[I]; }
    int FractionDigits = 0;
    if (Pos < Text.size() && Text[Pos] == Char('.'))
    {
        const auto FractionStart = ++Pos;
        while (Pos < Text.size() && Digit(Text[Pos])) { Digits[Count++] = Text[Pos++]; ++FractionDigits; }
        if (Pos == FractionStart) { return false; }
    }
    int Exponent = 0;
    if (Pos < Text.size() && (Text[Pos] == Char('e') || Text[Pos] == Char('E')))
    {
        ++Pos; bool NegativeExponent = false;
        if (Pos < Text.size() && (Text[Pos] == Char('+') || Text[Pos] == Char('-'))) { NegativeExponent = Text[Pos++] == Char('-'); }
        const auto ExponentStart = Pos;
        while (Pos < Text.size() && Digit(Text[Pos]))
        { if (Exponent > 10000) { return false; } Exponent = Exponent * 10 + int(Text[Pos++] - Char('0')); }
        if (Pos == ExponentStart) { return false; }
        if (NegativeExponent) { Exponent = -Exponent; }
    }
    if (Pos != Text.size()) { return false; }
    std::size_t Leading = 0; while (Leading < Count && Digits[Leading] == Char('0')) { ++Leading; }
    if (Leading == Count) { Out = 0; return true; }
    int Scale = Exponent - FractionDigits;
    while (Scale < 0 && Count > Leading && Digits[Count - 1] == Char('0')) { --Count; ++Scale; }
    if (Scale < 0 || Count - Leading > 19 || Scale > 19 || Count - Leading + std::size_t(Scale) > 19) { return false; }
    const std::uint64_t Limit = Negative ? std::uint64_t(9223372036854775808ULL) : std::uint64_t(9223372036854775807ULL);
    std::uint64_t Magnitude = 0;
    auto Append = [&](unsigned D) { if (Magnitude > (Limit - D) / 10) { return false; } Magnitude = Magnitude * 10 + D; return true; };
    for (auto I = Leading; I < Count; ++I) { if (!Append(unsigned(Digits[I] - Char('0')))) { return false; } }
    for (int I = 0; I < Scale; ++I) { if (!Append(0)) { return false; } }
    Out = Negative ? -static_cast<std::int64_t>(Magnitude - 1) - 1 : static_cast<std::int64_t>(Magnitude);
    return true;
}
// 既有Progression十进制字符串合同：只接受可完整表达的有符号十进制整数，不接受空白、指数或溢出饱和值。
template<class Char> bool ParseDecimalInteger(std::basic_string_view<Char> Text, std::int64_t& Out)
{
    if (Text.empty() || Text.size() > 1024) { return false; }
    const bool Negative = Text[0] == Char('-');
    std::size_t I = Negative ? 1 : 0;
    if (I == Text.size()) { return false; }
    const std::uint64_t Limit = Negative ? 9223372036854775808ULL : 9223372036854775807ULL;
    std::uint64_t Magnitude = 0;
    for (; I < Text.size(); ++I)
    {
        if (Text[I] < Char('0') || Text[I] > Char('9')) { return false; }
        const unsigned D = unsigned(Text[I] - Char('0'));
        if (Magnitude > (Limit - D) / 10) { return false; }
        Magnitude = Magnitude * 10 + D;
    }
    Out = Negative && Magnitude != 0 ? -static_cast<std::int64_t>(Magnitude - 1) - 1 : static_cast<std::int64_t>(Magnitude);
    return true;
}
}

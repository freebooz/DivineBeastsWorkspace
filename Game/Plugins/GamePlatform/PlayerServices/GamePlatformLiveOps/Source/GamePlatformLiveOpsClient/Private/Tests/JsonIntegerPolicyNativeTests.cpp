#if defined(GP_INTEGER_NATIVE_TEST)
// 原生整数回归：覆盖金融最小单位/版本/数量的原始JSON token，不访问生产Provider；错误意味着会发生截断或溢出。
#include "../Transport/JsonIntegerPolicy.h"
#include <iostream>
int main()
{
    int Failures = 0;
    auto Check = [&](const char* Input, bool Expected, std::int64_t Value = 0)
    { std::int64_t Actual = 123; bool Accepted = GPIntegerPolicy::ParseJsonInteger(std::string_view(Input), Actual); if (Accepted != Expected || (Accepted && Actual != Value)) { std::cerr << Input << " failed\n"; ++Failures; } };
    Check("100.5", false); Check("1.5", false); Check("1e1000", false); Check("NaN", false); Check("Infinity", false);
    Check("9223372036854775808", false); Check("-9223372036854775809", false);
    Check("9223372036854775807", true, INT64_MAX); Check("-9223372036854775808", true, INT64_MIN);
    Check("9007199254740993", true, 9007199254740993LL); Check("100.0", true, 100); Check("1e3", true, 1000);
    Check("100e-2", true, 1); Check("101e-2", false); Check("-0", true, 0); Check("0e100", true, 0); Check("01", false);
    std::int64_t Parsed = 0;
    if (GPIntegerPolicy::ParseDecimalInteger(std::string_view("9223372036854775808"), Parsed)) { ++Failures; }
    if (!GPIntegerPolicy::ParseDecimalInteger(std::wstring_view(L"9223372036854775807"), Parsed) || Parsed != INT64_MAX) { ++Failures; }
    if (!GPIntegerPolicy::ParseDecimalInteger(std::string_view("00012"), Parsed) || Parsed != 12) { ++Failures; }
    if (!GPIntegerPolicy::ParseDecimalInteger(std::string_view("-0"), Parsed) || Parsed != 0) { ++Failures; }
    return Failures == 0 ? 0 : 1;
}

#endif

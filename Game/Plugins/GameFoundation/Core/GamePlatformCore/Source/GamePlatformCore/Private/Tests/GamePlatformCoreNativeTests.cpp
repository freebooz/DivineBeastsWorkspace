// 仅CMake原生目标编译本入口；UE由同目录的自动化测试覆盖反射适配。
#if defined(GAME_PLATFORM_CORE_NATIVE_TESTS)
#include "Parsing/GamePlatformCoreAlgorithms.h"
#include "Parsing/GamePlatformResultPolicy.h"
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
using namespace GamePlatformCore::Private;
void Require(bool bCondition, const char* Description)
{
    if (!bCondition)
    {
        std::cerr << "FAIL: " << Description << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void IdentityValid()
{
    Identity<char> Value;
    Require(ParseIdentity<char>("Game.Platform.Hero_2@2147483647", Value), "mixed case/max version accepted");
    Require(Value.Namespace == "game.platform" && Value.Name == "hero_2", "ASCII normalization");
    Require(Value.LogicalVersion == 2147483647, "max version preserved");
    Require(FormatIdentity(Value) == "game.platform.hero_2@2147483647", "canonical output");
    Identity<char> RoundTrip;
    Require(ParseIdentity<char>(FormatIdentity(Value), RoundTrip) && EqualIdentity(Value, RoundTrip), "round trip");
    Require(!IsValidIdentity(Identity<char>{}), "default identity is invalid");
    Require(FormatIdentity(Identity<char>{}).empty(), "invalid format is empty");
}

void IdentityInvalid()
{
    // 移除任一资格校验会让对应非法输入被接受；失败不得泄漏上次身份。
    for (const std::string Text : {"", "a@1", ".a@1", "a.@1", "a..b@1", "a.b@0", "a.b@01", "a.b@+1",
        "a.b@-1", "a.b@2147483648", "a.b@99999999999999999999", "a.b@1@2", "a.b@1 ", " a.b@1",
        "a.b@", "a.1b@1", "_a.b@1", "a.b-c@1", "a/b.c@1", "a.b@1.0", "a.b@ 1"})
    {
        Identity<char> Value{"previous", "value", 5};
        Require(!ParseIdentity<char>(Text, Value), Text.c_str());
        Require(Value.Namespace.empty() && Value.Name.empty() && Value.LogicalVersion == 1, "failure clears all fields");
    }
    Identity<char> Value;
    Require(!ParseIdentity<char>(std::string("a.b@1\0hidden", 12), Value), "embedded NUL rejected");
    Require(!ParseIdentity<char>("a.\xc3\xa9@1", Value), "non ASCII rejected");
    Require(!IsValidIdentity(Identity<char>{"a", "b.c", 1}), "editable name cannot inject namespace");
    Require(!IsValidIdentity(Identity<char>{"a", "b", -1}), "editable negative version rejected");
}

void IdentityLimits()
{
    Identity<char> Value;
    Require(ParseIdentity<char>(std::string(64, 'a') + "." + std::string(64, 'b') + "@1", Value), "64 char segments");
    Require(!ParseIdentity<char>(std::string(65, 'a') + ".b@1", Value), "65 char namespace segment rejected");
    Require(!ParseIdentity<char>("a." + std::string(65, 'b') + "@1", Value), "65 char name rejected");
    const std::string AtLimit = std::string(64, 'a') + "." + std::string(64, 'b') + "." + std::string(60, 'c') + "@1";
    Require(AtLimit.size() == 192 && ParseIdentity<char>(AtLimit, Value), "192 char identity accepted");
    Require(!ParseIdentity<char>(AtLimit.substr(0, 190) + "c@1", Value), "193 char identity rejected");
}

void IdentityEquality()
{
    const Identity<char> Upper{"GAME.Platform", "Hero", 1};
    const Identity<char> Lower{"game.platform", "hero", 1};
    Require(IsValidIdentity(Upper) && EqualIdentity(Upper, Lower), "editable uppercase canonical equality");
    Require(HashIdentity(Upper) == HashIdentity(Lower), "equal canonical values hash equal");
    Require(!EqualIdentity(Lower, Identity<char>{"game.platform", "hero", 2}), "logical version belongs to identity");
    Require(!EqualIdentity(Identity<char>{"a", "b.c", 1}, Identity<char>{"a.b", "c", 1}), "invalid raw identity cannot impersonate valid identity");
    const Identity<char> Invalid{"BAD!", "a", 0};
    Require(EqualIdentity(Invalid, Invalid), "invalid reflexivity");
    Require(HashIdentity(Invalid) == HashIdentity(Identity<char>{"BAD!", "a", 0}), "invalid equal hash");
    Require(!EqualIdentity(Invalid, Identity<char>{"bad!", "a", 0}), "invalid values use exact raw fields");
}

void IdentityAlias()
{
    // 输入视图可指向输出字段；先清空输出会破坏输入，必须先完成读取。
    Identity<char> Value{"A.B@2", "old", 1};
    Require(ParseIdentity<char>(Value.Namespace, Value), "aliased parse input");
    Require(Value.Namespace == "a" && Value.Name == "b" && Value.LogicalVersion == 2, "aliased fields preserved until read");
}

void VersionValid()
{
    Version Value;
    Require(IsValidVersion(Value) && FormatVersion<char>(Value) == "0.0.0", "zero default is valid");
    Require(ParseVersion<char>("2147483647.2147483647.2147483647", Value), "all components max");
    Require(FormatVersion<char>(Value) == "2147483647.2147483647.2147483647", "max format");
    Version RoundTrip;
    Require(ParseVersion<char>(FormatVersion<char>(Value), RoundTrip) && CompareVersion(Value, RoundTrip) == 0, "version round trip");
    Require(!IsValidVersion(Version{-1, 0, 0}) && FormatVersion<char>(Version{-1, 0, 0}).empty(), "invalid editable version");
}

void VersionInvalid()
{
    for (const std::string Text : {"", "1", "1.2", "1.2.3.4", "1..3", ".1.2", "1.2.", "01.2.3", "1.02.3", "1.2.03",
        "+1.2.3", "-1.2.3", "1.-2.3", "1.2.-3", " 1.2.3", "1.2.3 ", "1.2.3-alpha", "1.2.3+build",
        "2147483648.0.0", "0.2147483648.0", "0.0.2147483648", "9999999999999999999999999.0.0"})
    {
        Version Value{1, 2, 3};
        Require(!ParseVersion<char>(Text, Value), Text.c_str());
        Require(Value.Major == 0 && Value.Minor == 0 && Value.Patch == 0, "version failure clears output");
    }
    Version Value;
    Require(!ParseVersion<char>(std::string("1.2.3\0tail", 10), Value), "version NUL rejected");
}

void VersionOrder()
{
    Require(CompareVersion(Version{2, 0, 0}, Version{1, 2147483647, 2147483647}) == 1, "major priority");
    Require(CompareVersion(Version{1, 2, 0}, Version{1, 1, 2147483647}) == 1, "minor priority");
    Require(CompareVersion(Version{1, 2, 3}, Version{1, 2, 4}) == -1, "patch ordering");
    Require(CompareVersion(Version{0, 0, 0}, Version{0, 0, 0}) == 0, "equality");
    Require(CompareVersion(Version{-2147483647 - 1, 0, 0}, Version{2147483647, 0, 0}) == -1, "comparison does not subtract and overflow");
}

void WideCharacters()
{
    Identity<wchar_t> Wide;
    Require(ParseIdentity<wchar_t>(L"CORE.Item@1", Wide) && FormatIdentity(Wide) == L"core.item@1", "wide production path");
    Require(!ParseIdentity<wchar_t>(L"core.名称@1", Wide), "wide non ASCII rejected");
    Identity<char16_t> Utf16;
    Require(ParseIdentity<char16_t>(u"CORE.Item@1", Utf16) && FormatIdentity(Utf16) == u"core.item@1", "UTF16 path");
    Require(HashIdentity(Utf16) == HashIdentity(Identity<char>{"core", "item", 1}), "hash independent of character width");
    Version Value;
    Require(ParseVersion<wchar_t>(L"1.2.3", Value) && FormatVersion<wchar_t>(Value) == L"1.2.3", "wide version");
}

void ResultStates()
{
    Require(!Result{}.IsSuccess(), "default result must not succeed");
    const Result Success = MakeResult(ResultStatus::Succeeded, "", "");
    Require(Success.IsSuccess() && Success.Code.empty() && Success.Message.empty(), "explicit success");
    Require(!Result{ResultStatus::Succeeded, "error", ""}.IsSuccess(), "contradictory success fails closed");
    const Result Failure = MakeResult(ResultStatus::Failed, "LoadFailed", "missing asset");
    Require(!Failure.IsSuccess() && Failure.Status == ResultStatus::Failed && Failure.Code == "LoadFailed" && Failure.Message == "missing asset", "failure retains diagnosis");
    const Result Cancelled = MakeResult(ResultStatus::Cancelled, "", "user stop");
    Require(!Cancelled.IsSuccess() && Cancelled.Status == ResultStatus::Cancelled && Cancelled.Code == "Cancelled" && Cancelled.Message == "user stop", "cancellation distinct from failure");
    const Result Unsupported = MakeResult(ResultStatus::Unsupported, "NoCapability", "not available");
    Require(!Unsupported.IsSuccess() && Unsupported.Status == ResultStatus::Unsupported && Unsupported.Code == "NoCapability", "unsupported distinct from failure");
}

void ResultDiagnostics()
{
    const Result Failure = MakeResult(ResultStatus::Failed, "", "original detail");
    Require(Failure.Code == "MissingFailureCode" && !Failure.Message.empty() && Failure.Message.find("original detail") != std::string::npos && !Failure.IsSuccess(), "missing failure code diagnosed without losing message");
    const Result Unsupported = MakeResult(ResultStatus::Unsupported, "", "");
    Require(Unsupported.Code == "MissingUnsupportedCode" && !Unsupported.Message.empty(), "missing unsupported code diagnosed");
    Require(!MakeResult(ResultStatus::Failed, "LoadFailed", "").Message.empty(), "empty failure message supplied");
    Require(!MakeResult(ResultStatus::Cancelled, "", "").Message.empty(), "empty cancellation message supplied");
    Require(!MakeResult(ResultStatus::Unsupported, "NoCapability", "").Message.empty(), "empty unsupported message supplied");
    const std::string Embedded("before\0after", 12);
    Require(MakeResult(ResultStatus::Failed, "Error", Embedded).Message == Embedded, "diagnostic byte length retained");
}
}

int main(int ArgCount, char** Arguments)
{
    if (ArgCount != 2) { return EXIT_FAILURE; }
    const std::string Scenario = Arguments[1];
    if (Scenario == "identity_valid") { IdentityValid(); }
    else if (Scenario == "identity_invalid") { IdentityInvalid(); }
    else if (Scenario == "identity_limits") { IdentityLimits(); }
    else if (Scenario == "identity_equality") { IdentityEquality(); }
    else if (Scenario == "identity_alias") { IdentityAlias(); }
    else if (Scenario == "version_valid") { VersionValid(); }
    else if (Scenario == "version_invalid") { VersionInvalid(); }
    else if (Scenario == "version_order") { VersionOrder(); }
    else if (Scenario == "wide_characters") { WideCharacters(); }
    else if (Scenario == "result_states") { ResultStates(); }
    else if (Scenario == "result_diagnostics") { ResultDiagnostics(); }
    else { return EXIT_FAILURE; }
    std::cout << "PASS: " << Scenario << '\n';
    return EXIT_SUCCESS;
}
#endif

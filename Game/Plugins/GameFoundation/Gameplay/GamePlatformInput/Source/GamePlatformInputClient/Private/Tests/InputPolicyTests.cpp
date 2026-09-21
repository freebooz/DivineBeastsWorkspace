// 原生入口只由独立 CMake 显式启用；UE 扫描此文件时不引入 main 或测试依赖。
#if defined(GAMEPLATFORM_INPUT_NATIVE_TESTS)
#include "Policy/InputPolicy.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

namespace
{
using namespace GamePlatformInputPolicy;
std::size_t AssertionCount = 0;
std::size_t FailureCount = 0;

// 不用会被 NDEBUG 去除的 assert；Release 同样执行全部断言并返回非零失败码。
void Check(bool bCondition, const char* Description)
{
    ++AssertionCount;
    if (!bCondition)
    {
        ++FailureCount;
        std::cerr << "FAIL: " << Description << '\n';
    }
}

void TestContextOwnership()
{
    ContextLedger Ledger;
    Check(!Ledger.EffectivePriority("Move"), "absent context has no priority");
    Check(!Ledger.Acquire(0, "Move", 10, false), "zero identity rejected");
    Check(!Ledger.Acquire(1, "", 10, false), "empty context rejected");
    Check(!Ledger.Acquire(1, "Move", 10, true), "external context takeover rejected");
    Check(!Ledger.Release(1), "failed acquire creates no lease");
    Check(Ledger.Acquire(1, "Move", -9, false), "negative priority accepted");
    Check(Ledger.Acquire(2, "Move", 20, false), "shared context accepted");
    Check(Ledger.Acquire(3, "Move", 20, false), "equal priority shared");
    Check(Ledger.Acquire(4, "Look", 5, false), "independent context");
    Check(Ledger.EffectivePriority("Move") == 20, "maximum priority selected");
    Check(!Ledger.Acquire(1, "Move", -9, false), "duplicate identity rejected without refcount");
    Check(!Ledger.Acquire(1, "Look", 99, false), "identity cannot overwrite another context");
    Check(!Ledger.Acquire(5, "Move", 99, true), "external conflict does not mutate shared context");
    Check(Ledger.EffectivePriority("Move") == 20, "conflicts preserve maximum");
    Check(!Ledger.Release(999), "foreign identity cannot release");
    Check(Ledger.Release(2), "release first high priority");
    Check(!Ledger.Release(2), "duplicate release is a no-op");
    Check(Ledger.EffectivePriority("Move") == 20, "equal priority owner retained");
    Check(Ledger.Release(3), "release remaining high priority");
    Check(Ledger.EffectivePriority("Move") == -9, "priority restores surviving negative value");
    Check(Ledger.Release(1), "last owner releases context");
    Check(!Ledger.EffectivePriority("Move"), "last release removes only owned context");
    Check(Ledger.EffectivePriority("Look") == 5, "other context retained");
    Check(Ledger.Release(4), "other owner releases separately");
    Check(Ledger.Acquire(0x100000001ULL, "High", (std::numeric_limits<int>::min)(), false), "64 bit identity and minimum priority");
    Check(Ledger.Acquire(1, "Low", (std::numeric_limits<int>::max)(), false), "upper token bits are significant");
    Check(Ledger.Release(1), "release low identity");
    Check(Ledger.EffectivePriority("High") == (std::numeric_limits<int>::min)(), "high identity unaffected");
    std::string Context = "OwnedCopy";
    Check(Ledger.Acquire(6, Context, 3, false), "context copied into ledger");
    Context = "Changed";
    Check(Ledger.EffectivePriority("OwnedCopy") == 3, "caller string lifetime independent");
    ContextLedger OtherPlayer;
    Check(!OtherPlayer.Release(6), "separate player does not see owner token");
    Check(Ledger.EffectivePriority("OwnedCopy") == 3, "separate player release has no effect");
}

void TestContextCapacity()
{
    ContextLedger Ledger;
    for (std::uint64_t Token = 1; Token <= 64; ++Token)
    {
        Check(Ledger.Acquire(Token, "Shared", static_cast<int>(Token), false), "64 shared leases accepted");
    }
    Check(!Ledger.Acquire(65, "Overflow", 999, false), "65th lease rejected");
    Check(!Ledger.EffectivePriority("Overflow"), "overflow has no partial context");
    Check(!Ledger.Acquire(1, "Changed", 999, false), "full ledger duplicate cannot overwrite");
    Check(Ledger.Release(64), "release highest priority at capacity");
    Check(Ledger.EffectivePriority("Shared") == 63, "capacity release restores priority");
    Check(Ledger.Acquire(65, "Replacement", 7, false), "released slot reusable with new identity");
    Check(!Ledger.Release(64), "stale token cannot release reused slot");
    Check(Ledger.EffectivePriority("Replacement") == 7, "replacement survives stale release");
    for (std::uint64_t Token = 1; Token <= 63; ++Token)
    {
        Check(Ledger.Release(Token), "all surviving shared owners release");
    }
    Check(!Ledger.EffectivePriority("Shared"), "shared context fully drained");
}

void TestBlocks()
{
    BlockLedger Ledger;
    Check(!Ledger.Acquire(0, 1), "block zero identity rejected");
    Check(!Ledger.Acquire(1, 0), "empty block rejected");
    Check(!Ledger.IsBlocked(0), "empty query never blocked");
    Check(Ledger.Acquire(1, 3), "loading blocks two channels");
    Check(Ledger.Acquire(2, 2), "menu overlaps look channel");
    Check(!Ledger.Acquire(1, 4), "duplicate cannot replace mask");
    Check(!Ledger.IsBlocked(4), "UI channel remains usable");
    Check(Ledger.IsBlocked(5), "any overlapping bit blocks composite query");
    Check(!Ledger.Release(55), "unknown block release is harmless");
    Check(Ledger.Release(1), "release loading before menu");
    Check(!Ledger.Release(1), "repeat release does not decrement menu");
    Check(!Ledger.IsBlocked(1), "move restored independently");
    Check(Ledger.IsBlocked(2), "menu still blocks look");
    Check(Ledger.Release(2), "menu release");
    Check(!Ledger.IsBlocked(3), "all blocks removed");
    for (std::uint64_t Token = 1; Token <= 64; ++Token)
    {
        Check(Ledger.Acquire(Token, 0x80000000U), "64 high-bit blockers accepted");
    }
    Check(!Ledger.Acquire(65, 1), "65th blocker rejected");
    Check(!Ledger.IsBlocked(1), "failed blocker leaves no bit");
    Check(Ledger.Release(32), "out of order blocker release");
    Check(Ledger.Acquire(0x100000020ULL, 1), "64 bit replacement token");
    Check(!Ledger.Release(32), "stale block identity does not remove replacement");
    Check(Ledger.IsBlocked(1), "replacement block survives");
    for (std::uint64_t Token = 1; Token <= 64; ++Token)
    {
        if (Token != 32) Check(Ledger.Release(Token), "remaining overlapping blockers release");
    }
    Check(!Ledger.IsBlocked(0x80000000U), "high bit cleared after last owner");
    Check(Ledger.IsBlocked(1), "replacement remains after draining other mask");
    Check(Ledger.Release(0x100000020ULL), "replacement releases");
    Check(!Ledger.IsBlocked(0xFFFFFFFFU), "ledger fully unblocked");
}

void TestActionLifecycle()
{
    ActionGate Gate;
    Check(!Gate.Observe(0, true, false), "idle terminal ignored");
    Check(!Gate.Observe(0.01, false, false), "idle deadzone ignored");
    Check(Gate.Observe(0.5, false, false), "analog action starts");
    Check(Gate.Observe(0.8, false, false), "continuous action updates");
    Check(Gate.Observe(0, false, false), "active axis can deliver zero");
    Check(Gate.Observe(0, true, false), "active terminal delivered once");
    Check(!Gate.Observe(0, true, false), "duplicate terminal suppressed");
    Check(Gate.Observe(1, false, false), "next action starts");
    Gate.Interrupt();
    Gate.Interrupt();
    Check(!Gate.Observe(1, false, false), "interrupted held action cannot restart");
    Check(!Gate.Observe(1, true, false), "stale nonneutral terminal cannot rearm");
    Check(!Gate.Observe(0.010001, false, false), "above deadzone still disarmed");
    Check(!Gate.Observe(0.01, true, false), "neutral terminal rearms without delivery");
    Check(Gate.Observe(0.010001, false, false), "new motion after neutral accepted");
    ActionGate OtherAction;
    Check(OtherAction.Observe(1, false, false), "independent action starts concurrently");
    Gate.Interrupt();
    Check(OtherAction.Observe(0, true, false), "other action terminal unaffected by interrupt");
    Check(!Gate.Observe(0, true, false), "interrupted action terminal suppressed");
}

void TestSuppressionAndInvalidSamples()
{
    ActionGate Gate;
    Check(Gate.Observe(1, false, false), "held before menu");
    Check(!Gate.Observe(1, false, true), "blocked flush trigger suppressed");
    Check(!Gate.Observe(0, true, true), "blocked flush terminal cannot rearm");
    Check(!Gate.Observe(0, false, true), "blocked synthetic neutral cannot rearm");
    Check(!Gate.Observe(1, false, false), "held after menu cannot restart");
    Check(!Gate.Observe(0, false, false), "unblocked neutral rearms without trigger");
    Check(Gate.Observe(1, false, false), "new press after menu accepted");
    for (double Invalid : {std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), -1.0})
    {
        Check(!Gate.Observe(Invalid, false, false), "invalid magnitude rejected and interrupts");
        Check(!Gate.Observe(1, false, false), "invalid input cannot act as neutral");
        Check(!Gate.Observe(Invalid, true, false), "invalid terminal cannot rearm");
        Check(!Gate.Observe(0, false, false), "valid neutral required after invalid sample");
        Check(Gate.Observe(1, false, false), "valid action after recovery");
    }
}

void TestAxisAndSettings()
{
    auto Axis = ClampAxis(0.3, -0.4);
    Check(Axis.first == 0.3 && Axis.second == -0.4, "analog magnitude preserved inside unit circle");
    Axis = ClampAxis(3, -4);
    Check(std::abs(Axis.first - 0.6) < 1e-12 && std::abs(Axis.second + 0.8) < 1e-12, "axis normalized preserving direction");
    Axis = ClampAxis(0, 1);
    Check(Axis.first == 0 && Axis.second == 1, "unit boundary unchanged");
    Axis = ClampAxis(0, 0);
    Check(Axis.first == 0 && Axis.second == 0, "zero axis unchanged");
    const double Largest = (std::numeric_limits<double>::max)();
    Axis = ClampAxis(Largest, Largest);
    Check(IsFiniteAxis(Axis.first, Axis.second), "large finite inputs do not overflow normalization");
    Check(std::abs(Axis.first - 0.7071067811865475) < 1e-12 && std::hypot(Axis.first, Axis.second) <= 1.0, "largest diagonal bounded and direction retained");
    Axis = ClampAxis(-Largest, 0);
    Check(Axis.first == -1 && Axis.second == 0, "largest negative cardinal axis");
    const double Tiny = std::numeric_limits<double>::denorm_min();
    Axis = ClampAxis(Tiny, -Tiny);
    Check(Axis.first == Tiny && Axis.second == -Tiny, "tiny analog values retained");
    Check(IsFiniteAxis(Largest, -Largest), "finite validation does not reject large values");
    for (double Invalid : {std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
    {
        Check(!IsFiniteAxis(Invalid, 0), "invalid first axis rejected");
        Check(!IsFiniteAxis(0, Invalid), "invalid second axis rejected");
        Axis = ClampAxis(Invalid, 0.5);
        Check(Axis.first == 0 && Axis.second == 0, "invalid first axis safely neutralized");
        Axis = ClampAxis(0.5, Invalid);
        Check(Axis.first == 0 && Axis.second == 0, "invalid second axis safely neutralized");
    }
    const auto Key = StableSettingsKey("Game", "opaque-user", 0, "Default");
    Check(!Key.empty(), "settings key available without IO");
    Check(Key == StableSettingsKey("Game", "opaque-user", 0, "Default"), "settings key repeatable");
    Check(Key != StableSettingsKey("PIE", "opaque-user", 0, "Default"), "PIE namespace isolated");
    Check(Key != StableSettingsKey("Game", "another-user", 0, "Default"), "opaque user isolated");
    Check(Key != StableSettingsKey("Game", "opaque-user", 1, "Default"), "local player isolated");
    Check(Key != StableSettingsKey("Game", "opaque-user", 0, "Alternate"), "profile isolated");
    Check(StableSettingsKey("a:b", "c", 0, "d") != StableSettingsKey("a", "b:c", 0, "d"), "field delimiter cannot collide");
    Check(StableSettingsKey("", "user", 0, "profile").empty(), "missing namespace rejected");
    Check(StableSettingsKey("Game", "", 0, "profile").empty(), "missing user rejected");
    Check(StableSettingsKey("Game", "user", 0, "").empty(), "missing profile rejected");
}
}

int main()
{
    TestContextOwnership();
    TestContextCapacity();
    TestBlocks();
    TestActionLifecycle();
    TestSuppressionAndInvalidSamples();
    TestAxisAndSettings();
    std::cout << "InputPolicyTests: " << AssertionCount << " assertions, " << FailureCount << " failures\n";
    return FailureCount == 0 ? 0 : 1;
}
#endif

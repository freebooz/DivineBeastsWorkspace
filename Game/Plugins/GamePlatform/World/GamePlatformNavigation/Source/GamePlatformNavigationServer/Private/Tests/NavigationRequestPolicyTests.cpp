#if defined(GAMEPLATFORM_NAVIGATION_NATIVE_TEST) && GAMEPLATFORM_NAVIGATION_NATIVE_TEST
// 原生行为回归只运行生产纯值策略，不代替UE导航系统的真实异步时序验收。
#include "../Queries/NavigationRequestPolicy.h"
#include <cstdlib>
#include <iostream>
using namespace GamePlatformNavigationRequestPolicy;
static void Require(bool Value, const char* Message)
{ if (!Value) { std::cerr << Message << '\n'; std::exit(1); } }
int main()
{
    // A正常完成后复用同一RequestId启动B，旧A句柄不能取消B；B当前回调仍应完成。
    std::uint64_t ActiveOperation = 1;
    const std::uint64_t HandleA = ActiveOperation;
    Require(MatchesTimeout(ActiveOperation, HandleA, 7, 7), "A completes its own operation");
    ActiveOperation = 2;
    if (MatchesHandle(ActiveOperation, HandleA, 7, 7)) { ActiveOperation = 0; }
    Require(ActiveOperation == 2, "old A cancel must leave B alive");
    Require(MatchesTimeout(ActiveOperation, 2, 7, 7), "B must still complete normally");
    Require(!MatchesHandle(2, 0, 7, 7), "legacy handle without generation must fail closed");
    Require(CanSchedule(0, 4, false), "normal request must be accepted");
    Require(!CanSchedule(1, 4, true), "duplicate in-flight ID must not replace original ownership");
    Require(!CanSchedule(4, 4, false), "full active budget must reject");
    Require(!MatchesTimeout(2, 1, 7, 7), "old timeout must not terminate reused request ID");
    Require(!MatchesTimeout(2, 2, 8, 7), "old world must not terminate new world operation");
    Require(MatchesTimeout(2, 2, 7, 7), "current timeout must terminate its operation");
}
#endif

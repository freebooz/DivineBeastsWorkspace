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
    Require(CanSchedule(0, 4, false), "normal request must be accepted");
    Require(!CanSchedule(1, 4, true), "duplicate in-flight ID must not replace original ownership");
    Require(!CanSchedule(4, 4, false), "full active budget must reject");
    Require(!MatchesTimeout(2, 1, 7, 7), "old timeout must not terminate reused request ID");
    Require(!MatchesTimeout(2, 2, 8, 7), "old world must not terminate new world operation");
    Require(MatchesTimeout(2, 2, 7, 7), "current timeout must terminate its operation");
}
#endif

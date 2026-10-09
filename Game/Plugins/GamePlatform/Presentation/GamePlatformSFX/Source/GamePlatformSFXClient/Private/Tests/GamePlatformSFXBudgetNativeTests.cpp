// SFX总槽位回归：255个活动加1个等待已经占满256；等待转活动不能额外突破预算。
#if defined(GAMEPLATFORM_SFX_NATIVE_TEST)
#include "../Policy/GamePlatformSFXBudgetPolicy.h"
#include <iostream>
int main()
{
    if (FGamePlatformSFXBudgetPolicy::CanReserve(1,255)) { std::cerr << "Pending加Active预算突破\n"; return 1; }
    if (!FGamePlatformSFXBudgetPolicy::CanReserve(0,255)) return 2;
    if (FGamePlatformSFXBudgetPolicy::CanReserve(128,0)) return 3;
    return 0;
}
#endif

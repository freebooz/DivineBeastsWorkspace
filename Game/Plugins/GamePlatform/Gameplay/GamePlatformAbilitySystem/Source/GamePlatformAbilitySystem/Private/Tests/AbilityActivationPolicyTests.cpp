#if defined(GAMEPLATFORM_ABILITY_NATIVE_TEST) && GAMEPLATFORM_ABILITY_NATIVE_TEST
// 验证缺Gate、旧Avatar和未绑定状态拒绝。真实GAS激活与复制资格由UE自动化补证。
#include "../Activation/AbilityActivationPolicy.h"
#include <cstdlib>
#include <iostream>
static void Require(bool Value, const char* Message)
{ if (!Value) { std::cerr << Message << '\n'; std::exit(1); } }
int main()
{
    using namespace GamePlatformAbilityActivationPolicy;
    Require(!CanEvaluate(true, false, 1, 1), "missing activation gate must fail closed");
    Require(!CanEvaluate(true, true, 2, 1), "old avatar gate must fail closed");
    Require(!CanEvaluate(false, true, 1, 1), "unbound avatar must fail closed");
    Require(CanEvaluate(true, true, 2, 2), "current avatar gate may evaluate gameplay Active");
}
#endif

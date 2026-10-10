#if defined(GAMEPLATFORM_AI_NATIVE_TEST) && GAMEPLATFORM_AI_NATIVE_TEST
// AI不能被玩家Gate永久禁用；已持资源/Brain/占有/代次真实就绪才允许，测试仅使用值输入。
#include "../Abilities/AIAbilityActivationPolicy.h"
#include <cstdlib>
#include <iostream>
static void Require(bool Value, const char* Message)
{ if (!Value) { std::cerr << Message << '\n'; std::exit(1); } }
int main()
{
    using namespace GamePlatformAIAbilityPolicy;
    Require(CanActivate(true,true,true,true,true,7,7,3,3), "ready authority AI must reach GAS activation gate");
    Require(!CanActivate(true,true,false,true,true,7,7,3,3), "AI before Brain initialization must reject");
    Require(!CanActivate(true,true,true,false,true,7,7,3,3), "released resource lease must reject");
    Require(!CanActivate(true,false,true,true,true,7,7,3,3), "lost possession or changed avatar must reject");
    Require(!CanActivate(false,true,true,true,true,7,7,3,3), "client/editor AI must reject");
    Require(!CanActivate(true,true,true,true,false,7,7,3,3), "dead or controlled AI must reject");
    Require(!CanActivate(true,true,true,true,true,8,7,3,3), "old AI generation must reject");
    Require(!CanActivate(true,true,true,true,true,7,7,4,3), "old resource request must reject");
}
#endif

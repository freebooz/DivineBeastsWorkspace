#if defined(GAMEPLATFORM_AI_NATIVE_TEST) && GAMEPLATFORM_AI_NATIVE_TEST
// 编辑地图/预览/命令行不能建立运行AI；真实Editor世界不Spawn由UE回归补证。
#include "../Perception/AIWorldPolicy.h"
#include <cstdlib>
#include <iostream>
static void Require(bool Value, const char* Message)
{ if (!Value) { std::cerr << Message << '\n'; std::exit(1); } }
int main()
{
    using namespace GamePlatformAIWorldPolicy;
    Require(!CanRun(false, true, false), "editor/preview world must never spawn authority controller");
    Require(!CanRun(true, true, true), "commandlet must never start runtime AI");
    Require(!CanRun(true, false, false), "client world must not run authority AI");
    Require(CanRun(true, true, false), "authority Game/PIE world may run AI");
}
#endif

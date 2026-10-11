// 定义预检调用同一生产算法的初始化校验；只在定义/配置变化时执行。
#include "Environment/GamePlatformFaunaConfig.h"
#include "Environment/Fauna/GamePlatformFaunaBridge.h"
bool FGamePlatformFaunaConfig::Validate(FString& OutError) const
{
    GamePlatformFauna::FGamePlatformFaunaSimulation Candidate;
    return GamePlatformFaunaBridge::Initialize(Candidate,*this,OutError);
}

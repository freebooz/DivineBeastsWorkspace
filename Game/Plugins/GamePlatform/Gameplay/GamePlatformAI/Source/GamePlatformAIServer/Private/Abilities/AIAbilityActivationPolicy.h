#pragma once
// 服务器AI读取到的事实归约；不拥有UObject/复制状态，游戏线程真实读取在Controller内完成。
namespace GamePlatformAIAbilityPolicy
{
inline bool CanActivate(bool bAuthorityWorld, bool bOwnerAndAvatarCurrent, bool bDefinitionAndBrainReady,
    bool bResourcesHeld, bool bAliveAndUncontrolled, int CurrentGeneration, int ExpectedGeneration,
    int CurrentResourceGeneration, int ExpectedResourceGeneration)
{
    return bAuthorityWorld && bOwnerAndAvatarCurrent && bDefinitionAndBrainReady && bResourcesHeld &&
        bAliveAndUncontrolled && CurrentGeneration > 0 && CurrentGeneration == ExpectedGeneration &&
        CurrentResourceGeneration > 0 && CurrentResourceGeneration == ExpectedResourceGeneration;
}
}

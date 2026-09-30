#pragma once
#include <cstdint>
// ASC游戏线程资格预检纯值策略。投影当前Avatar和注入Gate的代次；GAS仍另外核对冷却、费用和标签。
namespace GamePlatformAbilityActivationPolicy
{
inline bool CanEvaluate(bool Bound, bool ProviderValid, std::int32_t AvatarGeneration, std::int32_t GateGeneration)
{ return Bound && ProviderValid && AvatarGeneration > 0 && AvatarGeneration == GateGeneration; }
}

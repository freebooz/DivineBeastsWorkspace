// SSLM寒冰箭与占位法术的客户端资格过滤：只决定可选视觉，不修改GAS授权、伤害或服务器逻辑。
#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "UObject/Package.h" // GetOutermost返回UPackage，显式引入完整类型，避免UBT统一头偶然遮蔽编译缺失。

/** 项目表现私有纯函数；仅供开发施法窗口与复制弹体适配共同使用，不建立第二套VFX服务。 */
namespace DivineBeastsPlaceholderMagicVisualPolicy
{
/** 仅接受正式Village地图或UE编辑器为同一地图生成的PIE副本；不凭关卡短名放宽其它世界。 */
inline bool IsVillagePackageName(const FString& PackageName)
{
    static const FString VillagePath(TEXT("/DBAWorldPack_Village/Maps/L_Village_Start"));
    static const FString PiePrefix(TEXT("/DBAWorldPack_Village/Maps/UEDPIE_"));
    if (PackageName == VillagePath) return true;
    if (!PackageName.StartsWith(PiePrefix)) return false;
    // UE PIE仅允许“非空数字实例序号_正式地图名”；其它相似文件名不得获得临时技能展示资格。
    const FString PieTail = PackageName.RightChop(PiePrefix.Len());
    const FString MapSuffix(TEXT("_L_Village_Start"));
    return PieTail.EndsWith(MapSuffix) && PieTail.Len() > MapSuffix.Len() &&
        PieTail.Left(PieTail.Len() - MapSuffix.Len()).IsNumeric();
}

/** 校验实际世界归属。不得在登录、竞技或角色工作室默认激活占位VFX。 */
inline bool IsVillageWorld(const UWorld* World)
{
    return World && IsVillagePackageName(World->GetOutermost()->GetName());
}

/** 只识别十二生肖本人的三类占位能力身份，不信任客户端传入的任意效果资源路径。
 * 输入Magic只能是Fireball/Frostbolt/ArcaneMissiles；返回true仅授权表现，不授予游戏技能。 */
inline bool IsApprovedPlaceholderSpell(FName AbilityId, FName Magic)
{
    if (AbilityId.IsNone() ||
        (Magic != TEXT("Fireball") && Magic != TEXT("Frostbolt") && Magic != TEXT("ArcaneMissiles")))
        return false;

    static const TCHAR* const Heroes[] = {
        TEXT("rat"), TEXT("ox"), TEXT("tiger"), TEXT("rabbit"),
        TEXT("dragon"), TEXT("snake"), TEXT("horse"), TEXT("goat"),
        TEXT("monkey"), TEXT("rooster"), TEXT("dog"), TEXT("boar")
    };
    const FString Suffix = FString(TEXT("_placeholder_")) + Magic.ToString().ToLower() + TEXT("@1");
    const FString Value = AbilityId.ToString();
    for (const TCHAR* Hero : Heroes)
        if (Value == FString(TEXT("dba.ability.")) + Hero + Suffix) return true;
    return false;
}
}

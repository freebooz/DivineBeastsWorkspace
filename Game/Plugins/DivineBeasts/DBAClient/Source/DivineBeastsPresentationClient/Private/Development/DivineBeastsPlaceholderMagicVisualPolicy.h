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

/** 精确识别十二生肖已批准的占位能力及临时槽位映射，不信任任意效果资源路径。
 * Magic限定为既有四类飞行法术或Healing；返回true仅授权表现，不授予游戏技能。 */
inline bool IsApprovedPlaceholderSpell(FName AbilityId, FName Magic)
{
    if (AbilityId.IsNone() ||
        (Magic != TEXT("Fireball") && Magic != TEXT("Frostbolt") && Magic != TEXT("ArcaneMissiles") && Magic != TEXT("DirectedMissile") && Magic != TEXT("Healing")))
        return false;

    // 用户批准的36个角色空槽映射；精确核对技能和样式，不能凭前缀放行任意资源。
    // 对应AuthorPlayableHotbarFallbacksUE58.py制作的真实授权资产，非V6最终机制签审。
    struct FApprovedMapping { const TCHAR* Ability; const TCHAR* Style; };
    static const FApprovedMapping Mappings[] = {
        {TEXT("dba.ability.rat_eight_mist_guard@1"), TEXT("Healing")},
        {TEXT("dba.ability.rat_eight_shadow_snare@1"), TEXT("Frostbolt")},
        {TEXT("dba.ability.rat_eight_fang_mark@1"), TEXT("ArcaneMissiles")},
        {TEXT("dba.ability.ox_eight_stone_armor@1"), TEXT("Healing")},
        {TEXT("dba.ability.ox_eight_seismic_stun@1"), TEXT("Frostbolt")},
        {TEXT("dba.ability.ox_eight_steadfast_roar@1"), TEXT("ArcaneMissiles")},
        {TEXT("dba.ability.tiger_eight_regal_guard@1"), TEXT("Healing")},
        {TEXT("dba.ability.tiger_eight_roar_shock@1"), TEXT("Frostbolt")},
        {TEXT("dba.ability.tiger_eight_battle_fervor@1"), TEXT("ArcaneMissiles")},
        {TEXT("dba.ability.rabbit_eight_jade_moon_shield@1"), TEXT("Healing")},
        {TEXT("dba.ability.rabbit_eight_lunar_bind@1"), TEXT("Frostbolt")},
        {TEXT("dba.ability.rabbit_eight_jade_heal@1"), TEXT("ArcaneMissiles")},
        {TEXT("dba.ability.dragon_eight_dragon_scale_guard@1"), TEXT("Healing")},
        {TEXT("dba.ability.dragon_eight_thunder_cloud_lock@1"), TEXT("Frostbolt")},
        {TEXT("dba.ability.dragon_eight_cloud_blessing@1"), TEXT("ArcaneMissiles")},
        {TEXT("dba.ability.snake_eight_shed_skin@1"), TEXT("Healing")},
        {TEXT("dba.ability.snake_eight_coil_silence@1"), TEXT("Frostbolt")},
        {TEXT("dba.ability.snake_eight_venom_mark@1"), TEXT("ArcaneMissiles")},
        {TEXT("dba.ability.horse_eight_thunder_hide@1"), TEXT("Healing")},
        {TEXT("dba.ability.horse_eight_hoof_stun@1"), TEXT("Frostbolt")},
        {TEXT("dba.ability.horse_eight_tailwind_aura@1"), TEXT("ArcaneMissiles")},
        {TEXT("dba.ability.goat_eight_jade_barrier@1"), TEXT("Healing")},
        {TEXT("dba.ability.goat_eight_jade_bind@1"), TEXT("Frostbolt")},
        {TEXT("dba.ability.goat_eight_healing_dew@1"), TEXT("ArcaneMissiles")},
        {TEXT("dba.ability.monkey_eight_illusion_guard@1"), TEXT("Healing")},
        {TEXT("dba.ability.monkey_eight_staff_stun@1"), TEXT("Frostbolt")},
        {TEXT("dba.ability.monkey_eight_trickster_stride@1"), TEXT("ArcaneMissiles")},
        {TEXT("dba.ability.rooster_eight_wing_guard@1"), TEXT("Healing")},
        {TEXT("dba.ability.rooster_eight_sonic_silence@1"), TEXT("Frostbolt")},
        {TEXT("dba.ability.rooster_eight_first_light_heal@1"), TEXT("ArcaneMissiles")},
        {TEXT("dba.ability.dog_eight_guardian_ring@1"), TEXT("Healing")},
        {TEXT("dba.ability.dog_eight_tracking_slow@1"), TEXT("Frostbolt")},
        {TEXT("dba.ability.dog_eight_loyal_heal@1"), TEXT("ArcaneMissiles")},
        {TEXT("dba.ability.boar_eight_iron_mane@1"), TEXT("Healing")},
        {TEXT("dba.ability.boar_eight_roar_stun@1"), TEXT("Frostbolt")},
        {TEXT("dba.ability.boar_eight_rage_focus@1"), TEXT("ArcaneMissiles")},
    };
    for (const auto& Mapping : Mappings)
        if (AbilityId == FName(Mapping.Ability)) return Magic == FName(Mapping.Style);

    static const TCHAR* const Heroes[] = {
        TEXT("rat"), TEXT("ox"), TEXT("tiger"), TEXT("rabbit"),
        TEXT("dragon"), TEXT("snake"), TEXT("horse"), TEXT("goat"),
        TEXT("monkey"), TEXT("rooster"), TEXT("dog"), TEXT("boar")
    };
    const FString Suffix = FString(TEXT("_placeholder_")) + (Magic == TEXT("Healing") ? FString(TEXT("primary")) : Magic.ToString().ToLower()) + TEXT("@1");
    const FString Value = AbilityId.ToString();
    for (const TCHAR* Hero : Heroes)
        if (Value == FString(TEXT("dba.ability.")) + Hero + Suffix) return true;
    return false;
}
}

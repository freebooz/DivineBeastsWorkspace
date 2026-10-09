#include "Tags/GamePlatformCombatTags.h"

namespace GamePlatformCombatTags
{
    UE_DEFINE_GAMEPLAY_TAG(Data_Damage, "Combat.Data.Damage");
    UE_DEFINE_GAMEPLAY_TAG(Data_Damage_AttackPowerCoefficient, "Combat.Data.Damage.AttackPowerCoefficient");
    UE_DEFINE_GAMEPLAY_TAG(Data_Damage_AbilityPowerCoefficient, "Combat.Data.Damage.AbilityPowerCoefficient");
    UE_DEFINE_GAMEPLAY_TAG(Data_Damage_Type, "Combat.Data.Damage.Type");
    // 历史标签保留名称以避免已保存资产引用无法反序列化；仅存量兼容，不参与当前战斗。
    UE_DEFINE_GAMEPLAY_TAG(Data_Damage_CanCritical, "Combat.Data.Damage.CanCritical");
    UE_DEFINE_GAMEPLAY_TAG(Data_Damage_CriticalRoll, "Combat.Data.Damage.CriticalRoll");
    UE_DEFINE_GAMEPLAY_TAG(Data_Healing, "Combat.Data.Healing");

    UE_DEFINE_GAMEPLAY_TAG(Damage_BypassShield, "Combat.Damage.BypassShield");

    UE_DEFINE_GAMEPLAY_TAG(State_Dead, "Combat.State.Dead");
    UE_DEFINE_GAMEPLAY_TAG(State_Shielded, "Combat.State.Shielded");

    UE_DEFINE_GAMEPLAY_TAG(Control_Stun, "Combat.Control.Stun");
    UE_DEFINE_GAMEPLAY_TAG(Control_Silence, "Combat.Control.Silence");

    UE_DEFINE_GAMEPLAY_TAG(Event_Damage, "Combat.Event.Damage");
    UE_DEFINE_GAMEPLAY_TAG(Event_Healing, "Combat.Event.Healing");
    UE_DEFINE_GAMEPLAY_TAG(Event_Control, "Combat.Event.Control");
    UE_DEFINE_GAMEPLAY_TAG(Event_Death, "Combat.Event.Death");

    // 已废弃的历史表现标识保留注册，不再由服务器结算发布。
    UE_DEFINE_GAMEPLAY_TAG(Result_Critical, "Combat.Result.Critical");
    UE_DEFINE_GAMEPLAY_TAG(Result_ControlResisted, "Combat.Result.ControlResisted");

    UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Damage, "GameplayCue.Combat.Damage");
    UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Healing, "GameplayCue.Combat.Healing");
    UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Control, "GameplayCue.Combat.Control");
    UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Death, "GameplayCue.Combat.Death");
}

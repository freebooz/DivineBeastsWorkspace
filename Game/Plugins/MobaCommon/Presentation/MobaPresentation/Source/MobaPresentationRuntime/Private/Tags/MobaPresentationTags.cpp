#include "Tags/MobaPresentationTags.h"

namespace MobaPresentationTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combat_Hit, "Moba.Combat.Hit", "MOBA确认命中表现语义。");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combat_Critical, "Moba.Combat.Critical", "MOBA暴击表现语义。");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combat_Heal, "Moba.Combat.Heal", "MOBA治疗表现语义。");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combat_Shield_Hit, "Moba.Combat.Shield.Hit", "MOBA护盾吸收命中表现语义。");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combat_Control_Apply, "Moba.Combat.Control.Apply", "MOBA控制施加表现语义。");

    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Cast_Start, "Moba.Ability.Cast.Start", "MOBA技能施法开始表现语义。");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Cast_Release, "Moba.Ability.Cast.Release", "MOBA技能释放表现语义。");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Projectile_Spawn, "Moba.Ability.Projectile.Spawn", "MOBA投射物表现生成语义。");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Area_Warning, "Moba.Ability.Area.Warning", "MOBA范围预警表现语义。");

    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Apply, "Moba.Status.Apply", "MOBA持续状态施加表现语义。");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Remove, "Moba.Status.Remove", "MOBA持续状态移除表现语义。");

    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Death, "Moba.Character.Death", "MOBA角色死亡表现语义。");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Respawn, "Moba.Character.Respawn", "MOBA角色复活表现语义。");

    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Arena_Match_Start, "Moba.Arena.Match.Start", "MOBA竞技比赛开始表现语义。");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Arena_Match_End, "Moba.Arena.Match.End", "MOBA竞技比赛结束表现语义。");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Arena_Score_Changed, "Moba.Arena.Score.Changed", "MOBA竞技比分变化表现语义。");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Arena_Objective_Completed, "Moba.Arena.Objective.Completed", "MOBA竞技目标完成表现语义。");
}

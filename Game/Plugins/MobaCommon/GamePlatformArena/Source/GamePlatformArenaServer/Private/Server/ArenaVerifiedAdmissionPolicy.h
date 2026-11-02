#pragma once
// 竞技准入边界：调用方必须先从ServerAdmission查到当前Controller真实绑定（含Boot/协议验证）；本规则只匹配可信比赛Roster。
#include "Arena/GamePlatformArenaTypes.h"
#include "Server/GamePlatformServerAdmissionSubsystem.h"
namespace GamePlatformArenaVerifiedAdmissionPolicy
{
/** 返回Assignment内借用槽位；Assignment必须先通过GameMode结构验证。
 * Admission必须来自当前Controller的GetVerifiedAdmission查询，该入口已精确验证实例/Boot/World/体验/协议。
 * 本层只匹配比赛及Roster，不能用某个项目的旧World名称覆盖已校验的真实目标。
 * 旧比赛/过期/不同实例体验与重复角色身份均拒绝。 */
inline const FGamePlatformArenaRosterSlot* ResolveRosterSlot(const FGamePlatformServerVerifiedAdmission& Admission,
    const FGamePlatformArenaAssignment& Assignment)
{
    if (!Admission.IsStructurallyValid() || Assignment.MatchId.IsEmpty() || Admission.MatchId != Assignment.MatchId ||
        Admission.ServerInstanceId != Assignment.GameServerId || Assignment.ServerRole != TEXT("GameServer.Role.MainArena") ||
        Admission.ExperienceId != Assignment.ExperienceId.ToString() ||
        Admission.AuthorityUntil <= FDateTime::UtcNow()) { return nullptr; }
    const FGamePlatformArenaRosterSlot* Selected = nullptr;
    for (const auto& Slot : Assignment.Roster)
    {
        if (Slot.PlayerId == Admission.PlayerId)
        { if (Selected || !Slot.IsValid()) { return nullptr; } Selected = &Slot; }
    }
    if (!Selected) { return nullptr; }
    for (const auto& Slot : Assignment.Roster)
    { if (&Slot != Selected && Slot.CharacterId == Selected->CharacterId) { return nullptr; } }
    return Selected;
}
}

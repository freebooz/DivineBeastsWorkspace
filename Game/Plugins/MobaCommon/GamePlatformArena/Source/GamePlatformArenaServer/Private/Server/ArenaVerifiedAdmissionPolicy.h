#pragma once
// 竞技准入边界：调用方必须先从ServerAdmission查到当前Controller真实绑定（含Boot/协议验证）；本规则只匹配可信比赛Roster。
#include "Arena/GamePlatformArenaTypes.h"
#include "Server/GamePlatformServerAdmissionSubsystem.h"
namespace GamePlatformArenaVerifiedAdmissionPolicy
{
/** 返回Assignment内借用槽位；Assignment必须先通过GameMode结构验证。旧比赛/过期/不同实例体验与重复角色身份均拒绝。 */
inline const FGamePlatformArenaRosterSlot* ResolveRosterSlot(const FGamePlatformServerVerifiedAdmission& Admission,
    const FGamePlatformArenaAssignment& Assignment)
{
    if (!Admission.IsStructurallyValid() || Assignment.MatchId.IsEmpty() || Admission.MatchId != Assignment.MatchId ||
        Admission.ServerInstanceId != Assignment.GameServerId || Assignment.ServerRole != TEXT("GameServer.Role.MainArena") ||
        Admission.ExperienceId != Assignment.ExperienceId.ToString() || Admission.WorldId != TEXT("World.MainArena") ||
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

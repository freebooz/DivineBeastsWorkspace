#include "MobaPresentationSemanticRegistry.h"

#include "Tags/MobaPresentationTags.h"

namespace
{
    FMobaPresentationSemanticDefinition MakeSemantic(
        const FGameplayTag& Tag,
        const TCHAR* Meaning,
        const TCHAR* SourceFact,
        bool bTransient)
    {
        FMobaPresentationSemanticDefinition Definition;
        Definition.Tag = Tag;
        Definition.SemanticMeaning = Meaning;
        Definition.SourceFact = SourceFact;
        Definition.bTransient = bTransient;
        return Definition;
    }
}

const TArray<FMobaPresentationSemanticDefinition>& FMobaPresentationSemanticRegistry::GetAll()
{
    static const TArray<FMobaPresentationSemanticDefinition> Definitions =
    {
        MakeSemantic(MobaPresentationTags::Combat_Hit, TEXT("确认命中"), TEXT("Combat Damage Result"), true),
        MakeSemantic(MobaPresentationTags::Combat_Heal, TEXT("治疗"), TEXT("Combat Healing Result"), true),
        MakeSemantic(MobaPresentationTags::Combat_Shield_Hit, TEXT("护盾吸收"), TEXT("Combat Shield Absorb"), true),
        MakeSemantic(MobaPresentationTags::Combat_Control_Apply, TEXT("控制施加"), TEXT("Combat ControlApplied"), true),
        MakeSemantic(MobaPresentationTags::Ability_Cast_Start, TEXT("技能施法开始"), TEXT("Ability CastStart"), true),
        MakeSemantic(MobaPresentationTags::Ability_Cast_Release, TEXT("技能释放"), TEXT("Ability CastRelease"), true),
        MakeSemantic(MobaPresentationTags::Ability_Projectile_Spawn, TEXT("投射物表现生成"), TEXT("Ability Projectile Fact"), true),
        MakeSemantic(MobaPresentationTags::Ability_Area_Warning, TEXT("范围预警"), TEXT("Ability Area Warning Fact"), true),
        MakeSemantic(MobaPresentationTags::Status_Apply, TEXT("状态施加"), TEXT("Replicated Status Apply"), false),
        MakeSemantic(MobaPresentationTags::Status_Remove, TEXT("状态移除"), TEXT("Replicated Status Remove"), true),
        MakeSemantic(MobaPresentationTags::Character_Death, TEXT("角色死亡"), TEXT("Combat/Character Death"), true),
        MakeSemantic(MobaPresentationTags::Character_Respawn, TEXT("角色复活"), TEXT("Character New Avatar Active"), true),
        MakeSemantic(MobaPresentationTags::Arena_Match_Start, TEXT("比赛开始"), TEXT("Arena MatchPhase InProgress"), true),
        MakeSemantic(MobaPresentationTags::Arena_Match_End, TEXT("比赛结束"), TEXT("Arena terminal MatchPhase"), true),
        MakeSemantic(MobaPresentationTags::Arena_Score_Changed, TEXT("比分变化"), TEXT("Arena TeamState Revision"), false),
        MakeSemantic(MobaPresentationTags::Arena_Objective_Completed, TEXT("目标完成"), TEXT("Arena Objective Revision"), true)
    };
    return Definitions;
}

const FMobaPresentationSemanticDefinition* FMobaPresentationSemanticRegistry::Find(
    const FGameplayTag& Tag)
{
    return GetAll().FindByPredicate([&Tag](const FMobaPresentationSemanticDefinition& Definition)
    {
        return Definition.Tag == Tag;
    });
}

bool FMobaPresentationSemanticRegistry::Validate(TArray<FString>& OutErrors)
{
    TSet<FGameplayTag> Seen;
    for (const FMobaPresentationSemanticDefinition& Definition : GetAll())
    {
        if (!Definition.Tag.IsValid())
        {
            OutErrors.Add(TEXT("存在未注册的MobaPresentation语义Tag。"));
            continue;
        }
        if (Seen.Contains(Definition.Tag))
        {
            OutErrors.Add(FString::Printf(TEXT("重复语义所有权：%s"), *Definition.Tag.ToString()));
        }
        Seen.Add(Definition.Tag);

        const FString Text = Definition.Tag.ToString();
        if (!Text.StartsWith(TEXT("Moba.")))
        {
            OutErrors.Add(FString::Printf(TEXT("非Moba语义前缀：%s"), *Text));
        }
        if (Text.StartsWith(TEXT("FiveCamp.")) ||
            Text.StartsWith(TEXT("Faction.")) ||
            Text.StartsWith(TEXT("Element.")) ||
            Text.StartsWith(TEXT("KingSeal.")))
        {
            OutErrors.Add(FString::Printf(TEXT("检测到已取消旧系统Tag：%s"), *Text));
        }
    }
    return OutErrors.IsEmpty();
}

bool FMobaPresentationSemanticRegistry::IsTransient(const FGameplayTag& Tag)
{
    if (const FMobaPresentationSemanticDefinition* Definition = Find(Tag))
    {
        return Definition->bTransient;
    }
    return true;
}

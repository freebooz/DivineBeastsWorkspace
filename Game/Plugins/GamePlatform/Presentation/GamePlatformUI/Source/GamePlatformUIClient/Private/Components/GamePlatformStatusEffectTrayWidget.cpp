#include "Components/GamePlatformStatusEffectTrayWidget.h"

namespace
{
/** 平台安全限制：用于避免错误上游快照让蓝图瞬间创建大量图标。 */
constexpr int32 MaxDisplayEffects = 64;
constexpr int32 MaxMechanicTagsPerEffect = 8;
constexpr int32 MaxStacks = 9999;
constexpr float MaxEffectDurationSeconds = 60.0f * 60.0f * 24.0f * 30.0f;

/** 重要性排序与稳定ID排序在单次事件到达时计算，避免多Widget重复操作。 */
bool SortEffects(const FGamePlatformUIStatusEffect& A,
    const FGamePlatformUIStatusEffect& B)
{
    if (A.Importance != B.Importance)
    {
        return static_cast<uint8>(A.Importance) > static_cast<uint8>(B.Importance);
    }
    // 永久效果排在具体剩余秒数之后，重要性级别仍先于到期时间。
    const float ATime = A.RemainingSeconds < 0.0f
        ? MAX_flt : A.RemainingSeconds;
    const float BTime = B.RemainingSeconds < 0.0f
        ? MAX_flt : B.RemainingSeconds;
    if (!FMath::IsNearlyEqual(ATime, BTime, 0.0001f))
    {
        return ATime < BTime;
    }
    const FString AKey = FGamePlatformUIStatusEffectPresentation::
        GetInstanceDisplayKey(A).ToString();
    const FString BKey = FGamePlatformUIStatusEffectPresentation::
        GetInstanceDisplayKey(B).ToString();
    return AKey < BKey;
}

/** 将超出配额的效果计入显式剩余数量，不可静默隐藏关键告警。 */
void TrimToCapacity(TArray<FGamePlatformUIStatusEffect>& List,
    int32 MaxVisible, int32& OutOverflow)
{
    List.StableSort(&SortEffects);
    const int32 Visible = FMath::Min(List.Num(), MaxVisible);
    OutOverflow = List.Num() - Visible;
    List.SetNum(Visible, EAllowShrinking::No);
}

/** DisplayPolicy（展示配额）不接受无效容量，关键告警必须保留非零显示入口。 */
bool IsValidPolicy(const FGamePlatformUIStatusEffectDisplayPolicy& Policy)
{
    return Policy.MaxBeneficial >= 0 && Policy.MaxBeneficial <= MaxDisplayEffects &&
        Policy.MaxHarmful >= 0 && Policy.MaxHarmful <= MaxDisplayEffects &&
        Policy.MaxCritical >= 1 && Policy.MaxCritical <= MaxDisplayEffects &&
        Policy.MaxOther >= 0 && Policy.MaxOther <= MaxDisplayEffects;
}
}

FName FGamePlatformUIStatusEffectPresentation::GetInstanceDisplayKey(
    const FGamePlatformUIStatusEffect& Effect)
{
    return Effect.EffectInstanceId.IsNone()
        ? Effect.EffectId : Effect.EffectInstanceId;
}

EGamePlatformUIEffectPolarity
FGamePlatformUIStatusEffectPresentation::GetDisplayPolarity(
    const FGamePlatformUIStatusEffect& Effect)
{
    return Effect.bHasExplicitPolarity
        ? Effect.Polarity
        : (Effect.bBeneficial
            ? EGamePlatformUIEffectPolarity::Beneficial
            : EGamePlatformUIEffectPolarity::Harmful);
}

bool FGamePlatformUIStatusEffectPresentation::BuildDisplayGroups(
    const FGamePlatformUIStatusEffectTrayState& InState,
    const FGamePlatformUIStatusEffectDisplayPolicy& Policy,
    FGamePlatformUIStatusEffectDisplayGroups& OutGroups)
{
    if (InState.OwnerDisplayId.IsNone() || InState.Revision < 0 ||
        InState.Effects.Num() > MaxDisplayEffects || !IsValidPolicy(Policy))
    {
        return false;
    }

    // 单次先验证所有条目，不向蓝图发布半截新状态。
    TSet<FName> SeenInstances;
    FGamePlatformUIStatusEffectDisplayGroups Candidate;
    for (const FGamePlatformUIStatusEffect& Item : InState.Effects)
    {
        const FName InstanceKey = GetInstanceDisplayKey(Item);
        if (Item.EffectId.IsNone() || InstanceKey.IsNone() ||
            SeenInstances.Contains(InstanceKey) ||
            Item.Stacks < 0 || Item.Stacks > MaxStacks ||
            !FMath::IsFinite(Item.RemainingSeconds) ||
            Item.RemainingSeconds < -1.0f ||
            Item.RemainingSeconds > MaxEffectDurationSeconds ||
            (Item.bHasExpirationTimeAnchor &&
                (!FMath::IsFinite(Item.ExpirationTimeAnchorSeconds) ||
                 Item.ExpirationTimeAnchorSeconds < 0.0)) ||
            Item.VisibleMechanicTags.Num() > MaxMechanicTagsPerEffect)
        {
            return false;
        }
        // 公共平台不解析内部Gameplay标签，只拒绝重复的非空显示标签。
        TSet<FName> SeenTags;
        for (const FName& Tag : Item.VisibleMechanicTags)
        {
            if (Tag.IsNone() || SeenTags.Contains(Tag))
            {
                return false;
            }
            SeenTags.Add(Tag);
        }
        SeenInstances.Add(InstanceKey);

        const bool bCritical = Item.bCriticalMechanic ||
            Item.Importance == EGamePlatformUIEffectImportance::CriticalMechanic ||
            Item.Importance == EGamePlatformUIEffectImportance::HardControl;
        if (bCritical)
        {
            Candidate.Critical.Add(Item);
            continue;
        }

        switch (GetDisplayPolarity(Item))
        {
        case EGamePlatformUIEffectPolarity::Beneficial:
            Candidate.Beneficial.Add(Item);
            break;
        case EGamePlatformUIEffectPolarity::Harmful:
            Candidate.Harmful.Add(Item);
            break;
        case EGamePlatformUIEffectPolarity::Neutral:
        case EGamePlatformUIEffectPolarity::Conditional:
            Candidate.Other.Add(Item);
            break;
        default:
            return false;
        }
    }

    TrimToCapacity(Candidate.Critical, Policy.MaxCritical,
        Candidate.CriticalOverflowCount);
    TrimToCapacity(Candidate.Harmful, Policy.MaxHarmful,
        Candidate.HarmfulOverflowCount);
    TrimToCapacity(Candidate.Beneficial, Policy.MaxBeneficial,
        Candidate.BeneficialOverflowCount);
    TrimToCapacity(Candidate.Other, Policy.MaxOther,
        Candidate.OtherOverflowCount);
    OutGroups = MoveTemp(Candidate);
    return true;
}

bool UGamePlatformStatusEffectTrayWidget::ApplyEffects(
    const FGamePlatformUIStatusEffectTrayState& InState)
{
    // 显式作用域下严禁接收旧账号或旧目标异步快照。
    // 没有显式绑定时保留仅含旧字段、SourceScopeId为空的兼容行为。
    if (BoundSourceScopeId.IsValid())
    {
        if (InState.SourceScopeId != BoundSourceScopeId ||
            InState.OwnerDisplayId != BoundOwnerDisplayId)
        {
            return false;
        }
    }
    else if (InState.SourceScopeId.IsValid())
    {
        // 新来源必须先BindDisplayContext，防止随意交替接收不同会话的事件。
        return false;
    }

    if (State.OwnerDisplayId == InState.OwnerDisplayId &&
        InState.Revision <= State.Revision)
    {
        return false;
    }

    FGamePlatformUIStatusEffectDisplayGroups Candidate;
    if (!FGamePlatformUIStatusEffectPresentation::BuildDisplayGroups(
        InState, DisplayPolicy, Candidate))
    {
        return false;
    }

    State = InState;
    Groups = MoveTemp(Candidate);
    BroadcastPresentation();
    return true;
}

bool UGamePlatformStatusEffectTrayWidget::BindDisplayContext(
    FName InOwnerDisplayId, FGuid InSourceScopeId)
{
    if (InOwnerDisplayId.IsNone() || !InSourceScopeId.IsValid())
    {
        return false;
    }
    if (BoundOwnerDisplayId == InOwnerDisplayId &&
        BoundSourceScopeId == InSourceScopeId)
    {
        return true;
    }

    BoundOwnerDisplayId = InOwnerDisplayId;
    BoundSourceScopeId = InSourceScopeId;
    State = FGamePlatformUIStatusEffectTrayState();
    Groups = FGamePlatformUIStatusEffectDisplayGroups();
    BroadcastPresentation();
    return true;
}

void UGamePlatformStatusEffectTrayWidget::ClearEffects()
{
    BoundSourceScopeId.Invalidate();
    BoundOwnerDisplayId = NAME_None;
    State = FGamePlatformUIStatusEffectTrayState();
    Groups = FGamePlatformUIStatusEffectDisplayGroups();
    BroadcastPresentation();
}

bool UGamePlatformStatusEffectTrayWidget::ApplyDisplayPolicy(
    const FGamePlatformUIStatusEffectDisplayPolicy& InPolicy)
{
    if (!IsValidPolicy(InPolicy))
    {
        return false;
    }
    // 无源状态也可调整预设：保留空图标，待下一次授权快照到来再生成分组。
    FGamePlatformUIStatusEffectDisplayGroups Candidate;
    if (!State.OwnerDisplayId.IsNone() &&
        !FGamePlatformUIStatusEffectPresentation::BuildDisplayGroups(
            State, InPolicy, Candidate))
    {
        return false;
    }

    DisplayPolicy = InPolicy;
    Groups = MoveTemp(Candidate);
    BroadcastPresentation();
    return true;
}

void UGamePlatformStatusEffectTrayWidget::BroadcastPresentation()
{
    BP_OnEffectsChanged(State);
    BP_OnEffectDisplayGroupsChanged(Groups);
}


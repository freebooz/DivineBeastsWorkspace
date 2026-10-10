#include "Layers/DivineBeastsRootLayout.h"

#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Components/GamePlatformStatusEffectTrayWidget.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "Tags/GamePlatformCombatTags.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Panels/Combat/DivineBeastsCombatPanelBase.h"
#include "Components/DivineBeastsPlayerPortraitWidget.h"
#include "Components/DivineBeastsVillageMinimapWidget.h"


namespace
{
/** 项目层只使用当前拥有者已复制的状态事实，投影为平台通用的效果展示快照。 */
FGamePlatformUIStatusEffect MakeOwnStatus(
    FName EffectId,
    const TCHAR* DisplayName,
    EGamePlatformUIEffectPolarity Polarity,
    EGamePlatformUIEffectMechanic Mechanic,
    EGamePlatformUIEffectImportance Importance)
{
    FGamePlatformUIStatusEffect Effect;
    Effect.EffectId = EffectId;
    Effect.DisplayName = FText::FromString(DisplayName);
    Effect.bHasExplicitPolarity = true;
    Effect.Polarity = Polarity;
    Effect.bBeneficial = Polarity == EGamePlatformUIEffectPolarity::Beneficial;
    Effect.PrimaryMechanic = Mechanic;
    Effect.Importance = Importance;
    Effect.RemainingSeconds = -1.0f; // 缺少可信倒计时数据时明确未知。
    return Effect;
}

/** 状态真正变化时才刷新最多六个文本图标；不引入Tick或新的视觉内容硬依赖。 */
void FillStatusRow(
    UGamePlatformStatusEffectTrayWidget* Panel,
    FName RowName,
    const TArray<FGamePlatformUIStatusEffect>& Items,
    const FLinearColor& Color)
{
    if (!IsValid(Panel))
    {
        return;
    }

    UWrapBox* Row = Cast<UWrapBox>(Panel->GetWidgetFromName(RowName));
    if (!IsValid(Row))
    {
        return;
    }

    Row->ClearChildren();
    for (const FGamePlatformUIStatusEffect& Item : Items)
    {
        UTextBlock* Label = NewObject<UTextBlock>(Panel);
        Label->SetText(Item.DisplayName);
        Label->SetColorAndOpacity(FSlateColor(Color));
        if (UWrapBoxSlot* Slot = Row->AddChildToWrapBox(Label))
        {
            Slot->SetPadding(FMargin(4.0f, 2.0f, 8.0f, 2.0f));
        }
    }
}

void UpdateOverflow(UGamePlatformStatusEffectTrayWidget* Panel, FName Name, int32 Count)
{
    if (IsValid(Panel))
    {
        if (UTextBlock* Text = Cast<UTextBlock>(Panel->GetWidgetFromName(Name)))
        {
            Text->SetText(Count > 0
                ? FText::FromString(FString::Printf(TEXT("+%d"), Count))
                : FText::GetEmpty());
        }
    }
}
}

void UDivineBeastsRootLayout::NativeConstruct()
{
    Super::NativeConstruct();

    if (APlayerController* Controller = GetOwningPlayer())
    {
        Controller->OnPossessedPawnChanged.AddUniqueDynamic(
            this, &UDivineBeastsRootLayout::HandlePossessedPawnChanged);
    }
    RefreshCombatHUDVisibility();
}

void UDivineBeastsRootLayout::NativeDestruct()
{
    if (APlayerController* Controller = GetOwningPlayer())
    {
        Controller->OnPossessedPawnChanged.RemoveDynamic(
            this, &UDivineBeastsRootLayout::HandlePossessedPawnChanged);
    }

    UnbindCombatEffects();
    if (IsValid(CombatHUD))
    {
        // 销毁根布局时明确撤销子组件的数据来源，避免旅行后旧Pawn委托仍访问上一界面。
        if (auto* Portrait = Cast<UDivineBeastsPlayerPortraitWidget>(CombatHUD->GetWidgetFromName(TEXT("PlayerPortrait"))))
        { Portrait->BindToPawn(nullptr); }
        if (auto* Minimap = Cast<UDivineBeastsVillageMinimapWidget>(CombatHUD->GetWidgetFromName(TEXT("Minimap"))))
        { Minimap->BindToPawn(nullptr); }
        CombatHUD->SetVisibility(ESlateVisibility::Collapsed);
    }
    Super::NativeDestruct();
}

void UDivineBeastsRootLayout::HandlePossessedPawnChanged(
    APawn* /*PreviousPawn*/, APawn* /*NewPawn*/)
{
    // 切换/断开角色立即撤销旧HUD投影；子面板自行解绑旧Pawn的GAS事件。
    RefreshCombatHUDVisibility();
}

void UDivineBeastsRootLayout::RefreshCombatHUDVisibility()
{
    if (!IsValid(CombatHUD))
    {
        return;
    }

    const APawn* CurrentPawn = GetOwningPlayerPawn();
    const bool bHasGameplayAvatar =
        IsValid(CurrentPawn) &&
        IsValid(CurrentPawn->FindComponentByClass<UGamePlatformAbilitySystemComponent>()) &&
        IsValid(CurrentPawn->FindComponentByClass<UGamePlatformCombatComponent>());

    // 仅本地玩家存在正式GAS战斗Pawn时显示。Boot/Login/CharacterSelect不展示空技能/假血条。
    // 专用服务器不创建此客户端Widget；不添加任何Gameplay权威、Tick或网络RPC。
    CombatHUD->SetVisibility(bHasGameplayAvatar
        ? ESlateVisibility::SelfHitTestInvisible
        : ESlateVisibility::Collapsed);

    // 世界加载/本地Pawn换代时，重新绑定效果投影并丢弃旧玩家的数据作用域。
    BindCombatEffects(bHasGameplayAvatar ? const_cast<APawn*>(CurrentPawn) : nullptr);
    // 组合根只为真实本地Pawn注入展示来源；项目肖像/小地图各自持有事件与纹理句柄，无MOBA依赖。
    APawn* DisplayPawn = bHasGameplayAvatar ? const_cast<APawn*>(CurrentPawn) : nullptr;
    if (auto* Portrait = Cast<UDivineBeastsPlayerPortraitWidget>(CombatHUD->GetWidgetFromName(TEXT("PlayerPortrait"))))
    {
        Portrait->SetOwningPlayer(GetOwningPlayer());
        Portrait->BindToPawn(DisplayPawn);
    }
    if (auto* Minimap = Cast<UDivineBeastsVillageMinimapWidget>(CombatHUD->GetWidgetFromName(TEXT("Minimap"))))
    {
        Minimap->SetOwningPlayer(GetOwningPlayer());
        Minimap->BindToPawn(DisplayPawn);
    }
}

void UDivineBeastsRootLayout::BindCombatEffects(APawn* Pawn)
{
    UAbilitySystemComponent* ASC = IsValid(Pawn)
        ? Pawn->FindComponentByClass<UGamePlatformAbilitySystemComponent>()
        : nullptr;
    UGamePlatformStatusEffectTrayWidget* Panel = IsValid(CombatHUD)
        ? Cast<UGamePlatformStatusEffectTrayWidget>(
            CombatHUD->GetWidgetFromName(TEXT("StatusEffects")))
        : nullptr;

    if (BoundEffectsASC.Get() == ASC && BoundEffectsWidget.Get() == Panel)
    {
        return;
    }
    UnbindCombatEffects();

    if (!IsValid(ASC) || !IsValid(Panel) || !IsValid(Pawn))
    {
        return;
    }

    BoundEffectsASC = ASC;
    BoundEffectsWidget = Panel;
    EffectDisplayScopeId = FGuid::NewGuid();
    EffectDisplayRevision = 0;
    LastEffectDisplaySignature.Reset();

    // SourceScopeId（显示来源代次）防止旧角色异步状态串到新角色。
    if (!Panel->BindDisplayContext(Pawn->GetFName(), EffectDisplayScopeId))
    {
        BoundEffectsASC.Reset();
        BoundEffectsWidget.Reset();
        return;
    }

    DamageBonusHandle = ASC->GetGameplayAttributeValueChangeDelegate(
        UGamePlatformCombatAttributeSet::GetDamageBonusAttribute()).AddUObject(
            this, &UDivineBeastsRootLayout::HandleCombatAttributeChanged);
    DamageReductionHandle = ASC->GetGameplayAttributeValueChangeDelegate(
        UGamePlatformCombatAttributeSet::GetDamageReductionAttribute()).AddUObject(
            this, &UDivineBeastsRootLayout::HandleCombatAttributeChanged);

    ShieldedTagHandle = ASC->RegisterGameplayTagEvent(
        GamePlatformCombatTags::State_Shielded,
        EGameplayTagEventType::NewOrRemoved).AddUObject(
            this, &UDivineBeastsRootLayout::HandleCombatTagChanged);
    StunTagHandle = ASC->RegisterGameplayTagEvent(
        GamePlatformCombatTags::Control_Stun,
        EGameplayTagEventType::NewOrRemoved).AddUObject(
            this, &UDivineBeastsRootLayout::HandleCombatTagChanged);
    SilenceTagHandle = ASC->RegisterGameplayTagEvent(
        GamePlatformCombatTags::Control_Silence,
        EGameplayTagEventType::NewOrRemoved).AddUObject(
            this, &UDivineBeastsRootLayout::HandleCombatTagChanged);

    RefreshCombatEffects();
}

void UDivineBeastsRootLayout::UnbindCombatEffects()
{
    if (UAbilitySystemComponent* ASC = BoundEffectsASC.Get())
    {
        if (DamageBonusHandle.IsValid())
        {
            ASC->GetGameplayAttributeValueChangeDelegate(
                UGamePlatformCombatAttributeSet::GetDamageBonusAttribute()).Remove(DamageBonusHandle);
        }
        if (DamageReductionHandle.IsValid())
        {
            ASC->GetGameplayAttributeValueChangeDelegate(
                UGamePlatformCombatAttributeSet::GetDamageReductionAttribute()).Remove(DamageReductionHandle);
        }
        if (ShieldedTagHandle.IsValid())
        {
            ASC->RegisterGameplayTagEvent(
                GamePlatformCombatTags::State_Shielded,
                EGameplayTagEventType::NewOrRemoved).Remove(ShieldedTagHandle);
        }
        if (StunTagHandle.IsValid())
        {
            ASC->RegisterGameplayTagEvent(
                GamePlatformCombatTags::Control_Stun,
                EGameplayTagEventType::NewOrRemoved).Remove(StunTagHandle);
        }
        if (SilenceTagHandle.IsValid())
        {
            ASC->RegisterGameplayTagEvent(
                GamePlatformCombatTags::Control_Silence,
                EGameplayTagEventType::NewOrRemoved).Remove(SilenceTagHandle);
        }
    }

    if (UGamePlatformStatusEffectTrayWidget* Panel = BoundEffectsWidget.Get())
    {
        Panel->ClearEffects();
        Panel->SetVisibility(ESlateVisibility::Collapsed);
    }

    BoundEffectsASC.Reset();
    BoundEffectsWidget.Reset();
    DamageBonusHandle.Reset();
    DamageReductionHandle.Reset();
    ShieldedTagHandle.Reset();
    StunTagHandle.Reset();
    SilenceTagHandle.Reset();
    EffectDisplayScopeId.Invalidate();
    EffectDisplayRevision = 0;
    LastEffectDisplaySignature.Reset();
}

void UDivineBeastsRootLayout::HandleCombatAttributeChanged(
    const FOnAttributeChangeData& /*ChangeData*/)
{
    RefreshCombatEffects();
}

void UDivineBeastsRootLayout::HandleCombatTagChanged(
    const FGameplayTag /*Tag*/, int32 /*NewCount*/)
{
    RefreshCombatEffects();
}

void UDivineBeastsRootLayout::RefreshCombatEffects()
{
    UAbilitySystemComponent* ASC = BoundEffectsASC.Get();
    UGamePlatformStatusEffectTrayWidget* Panel = BoundEffectsWidget.Get();
    const APawn* Pawn = GetOwningPlayerPawn();
    if (!IsValid(ASC) || !IsValid(Panel) || !IsValid(Pawn) ||
        ASC != Pawn->FindComponentByClass<UGamePlatformAbilitySystemComponent>() ||
        !EffectDisplayScopeId.IsValid())
    {
        return;
    }

    const bool bShielded = ASC->HasMatchingGameplayTag(GamePlatformCombatTags::State_Shielded);
    const bool bStunned = ASC->HasMatchingGameplayTag(GamePlatformCombatTags::Control_Stun);
    const bool bSilenced = ASC->HasMatchingGameplayTag(GamePlatformCombatTags::Control_Silence);

    const UGamePlatformCombatAttributeSet* Combat = ASC->GetSet<UGamePlatformCombatAttributeSet>();
    const float Bonus = Combat ? Combat->GetDamageBonus() : 0.0f;
    const float Reduction = Combat ? Combat->GetDamageReduction() : 0.0f;
    const int32 BonusSign = Bonus > KINDA_SMALL_NUMBER ? 1
        : (Bonus < -KINDA_SMALL_NUMBER ? -1 : 0);
    const int32 ReductionSign = Reduction > KINDA_SMALL_NUMBER ? 1
        : (Reduction < -KINDA_SMALL_NUMBER ? -1 : 0);

    // 只关心显示机制的改变，数值相同方向的变化无需反复分配UMG文本控件。
    const FString Signature = FString::Printf(
        TEXT("%d|%d|%d|%d|%d"),
        bShielded ? 1 : 0, bStunned ? 1 : 0, bSilenced ? 1 : 0,
        BonusSign, ReductionSign);
    if (Signature == LastEffectDisplaySignature)
    {
        return;
    }
    LastEffectDisplaySignature = Signature;

    FGamePlatformUIStatusEffectTrayState Snapshot;
    Snapshot.OwnerDisplayId = Pawn->GetFName();
    Snapshot.SourceScopeId = EffectDisplayScopeId;
    Snapshot.Revision = ++EffectDisplayRevision;

    if (bShielded)
    {
        Snapshot.Effects.Add(MakeOwnStatus(
            TEXT("Combat.State.Shielded"), TEXT("临时护盾"),
            EGamePlatformUIEffectPolarity::Beneficial,
            EGamePlatformUIEffectMechanic::Shield,
            EGamePlatformUIEffectImportance::Tactical));
    }
    if (bStunned)
    {
        Snapshot.Effects.Add(MakeOwnStatus(
            TEXT("Combat.Control.Stun"), TEXT("眩晕"),
            EGamePlatformUIEffectPolarity::Harmful,
            EGamePlatformUIEffectMechanic::Stun,
            EGamePlatformUIEffectImportance::HardControl));
    }
    if (bSilenced)
    {
        Snapshot.Effects.Add(MakeOwnStatus(
            TEXT("Combat.Control.Silence"), TEXT("沉默"),
            EGamePlatformUIEffectPolarity::Harmful,
            EGamePlatformUIEffectMechanic::Silence,
            EGamePlatformUIEffectImportance::HardControl));
    }
    if (BonusSign != 0)
    {
        Snapshot.Effects.Add(MakeOwnStatus(
            TEXT("Combat.Aggregate.DamageBonus"),
            BonusSign > 0 ? TEXT("伤害增强") : TEXT("伤害削弱"),
            BonusSign > 0
                ? EGamePlatformUIEffectPolarity::Beneficial
                : EGamePlatformUIEffectPolarity::Harmful,
            EGamePlatformUIEffectMechanic::None,
            EGamePlatformUIEffectImportance::Tactical));
    }
    if (ReductionSign != 0)
    {
        Snapshot.Effects.Add(MakeOwnStatus(
            TEXT("Combat.Aggregate.DamageReduction"),
            ReductionSign > 0 ? TEXT("伤害减免") : TEXT("受到易伤"),
            ReductionSign > 0
                ? EGamePlatformUIEffectPolarity::Beneficial
                : EGamePlatformUIEffectPolarity::Harmful,
            EGamePlatformUIEffectMechanic::None,
            EGamePlatformUIEffectImportance::Tactical));
    }

    if (Panel->ApplyEffects(Snapshot))
    {
        RenderCombatEffects();
    }
}

void UDivineBeastsRootLayout::RenderCombatEffects()
{
    UGamePlatformStatusEffectTrayWidget* Panel = BoundEffectsWidget.Get();
    if (!IsValid(Panel))
    {
        return;
    }

    const FGamePlatformUIStatusEffectDisplayGroups& Groups = Panel->GetDisplayGroupsView();
    // 不用“暴击”字义：Critical此处为玩家需及时注意的关键控制机制。
    FillStatusRow(Panel, TEXT("BuffItems"), Groups.Beneficial,
        FLinearColor(0.48f, 0.94f, 0.75f));
    FillStatusRow(Panel, TEXT("DebuffItems"), Groups.Harmful,
        FLinearColor(0.98f, 0.64f, 0.43f));
    FillStatusRow(Panel, TEXT("CriticalItems"), Groups.Critical,
        FLinearColor(1.0f, 0.80f, 0.35f));

    UpdateOverflow(Panel, TEXT("BuffOverflowText"), Groups.BeneficialOverflowCount);
    UpdateOverflow(Panel, TEXT("DebuffOverflowText"), Groups.HarmfulOverflowCount);
    UpdateOverflow(Panel, TEXT("CriticalOverflowText"), Groups.CriticalOverflowCount);

    const bool bHasVisibleStatus = !Groups.Beneficial.IsEmpty() ||
        !Groups.Harmful.IsEmpty() || !Groups.Critical.IsEmpty() ||
        !Groups.Other.IsEmpty();
    Panel->SetVisibility(bHasVisibleStatus
        ? ESlateVisibility::SelfHitTestInvisible
        : ESlateVisibility::Collapsed);
}



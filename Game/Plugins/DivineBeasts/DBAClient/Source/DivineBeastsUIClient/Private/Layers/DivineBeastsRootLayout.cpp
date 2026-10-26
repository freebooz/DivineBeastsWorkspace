// 项目客户端根布局：保存作者组合HUD，按本地控制器/Pawn事件重绑视图；平台Travel仍清理临时世界控件。
// 只恢复本根布局拥有的既有资产实例，退出解除旧委托与展示来源，不持有玩法权威或轮询世界。
#include "Layers/DivineBeastsRootLayout.h"
#include "Layers/DivineBeastsWorldHUDRoute.h"

#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Subsystems/GamePlatformCombatFeedbackWorldSubsystem.h"
#include "Types/GamePlatformCombatEvent.h"
#include "Adapters/Combat/DivineBeastsCombatUIFeedbackLibrary.h"
#include "Feedback/GamePlatformFeedbackWidget.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Components/GamePlatformCastProgressWidget.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Components/GamePlatformStatusEffectTrayWidget.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "Tags/GamePlatformCombatTags.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/LocalPlayer.h"
#include "Panels/Combat/DivineBeastsAbilityBarPanel.h"
#include "Panels/Combat/DivineBeastsPlayerStatusPanel.h"
#include "Panels/Combat/DivineBeastsCombatPanelBase.h"
#include "Components/DivineBeastsPlayerPortraitWidget.h"
#include "Components/DivineBeastsVillageMinimapWidget.h"
#include "DivineBeastsUIClientSubsystem.h"
#include "HUD/World/DivineBeastsWorldHUDBase.h"
#include "ViewModels/DivineBeastsUIViewModel.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Gameplay/DivineBeastsWorldPlayerController.h"
#include "WorldUI/DivineBeastsWorldHealthBarPresenter.h"
#include "WorldUI/GamePlatformWorldWidgetBase.h"
#include "UObject/StrongObjectPtr.h"


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
    bWorldHUDHostConstructed=true;
    RefreshForPlayerController(GetOwningPlayer());
}

void UDivineBeastsRootLayout::RefreshForPlayerController(APlayerController* Controller)
{
    if (APlayerController* Previous = BoundHUDController.Get(); Previous && Previous != Controller)
    {
        Previous->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
    }
    BoundHUDController = Controller;
    // 断开也必须传播空上下文，否则跨地图保留的控件仍可能读取上一控制器的Pawn。
    const FLocalPlayerContext Context = IsValid(Controller)
        ? FLocalPlayerContext(Controller) : FLocalPlayerContext();
    SetPlayerContext(Context);
    if (IsValid(CombatHUD)) { CombatHUD->SetPlayerContext(Context); }
    if (IsValid(Controller))
    {
        Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePossessedPawnChanged);
    }
    RefreshCombatHUDVisibility();
    ObserveWorldUIOwner();
    RefreshWorldContextHUD();
}

void UDivineBeastsRootLayout::NativeDestruct()
{
    bWorldHUDHostConstructed=false;bWorldHUDRefreshQueued=false;
    UnbindWorldCombatFeedback();
    BindWorldHealthBars(nullptr);
    if (auto* Owner=WorldUIOwner.Get()) Owner->OnStateChanged().Remove(WorldUIStateHandle);
    WorldUIStateHandle.Reset(); WorldUIOwner.Reset(); ReleaseWorldContextHUD();
    if (APlayerController* Controller = BoundHUDController.Get())
    {
        Controller->OnPossessedPawnChanged.RemoveDynamic(
            this, &UDivineBeastsRootLayout::HandlePossessedPawnChanged);
    }
    BoundHUDController.Reset();

    UnbindCombatEffects();
    UnbindCastDisplay();
    if (IsValid(CombatHUD))
    {
        // 销毁根布局时明确撤销子组件的数据来源，避免旅行后旧Pawn委托仍访问上一界面。
        // 顶部角色框与底部技能栏各自持有展示实例，失活时同时撤销，不能遗留其中一份旧角色。
        for (FName PortraitName : {FName(TEXT("PlayerPortrait")), FName(TEXT("ActionBarPortrait"))})
        {
            if (auto* Portrait = Cast<UDivineBeastsPlayerPortraitWidget>(CombatHUD->GetWidgetFromName(PortraitName)))
            { Portrait->BindToPawn(nullptr); }
        }
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
    RefreshWorldContextHUD();
}

void UDivineBeastsRootLayout::ObserveWorldUIOwner()
{
    auto* Owner=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsUIClientSubsystem>():nullptr;
    if(WorldUIOwner.Get()==Owner)return;
    if(auto* Previous=WorldUIOwner.Get())Previous->OnStateChanged().Remove(WorldUIStateHandle);
    WorldUIStateHandle.Reset(); ReleaseWorldContextHUD(); WorldUIOwner=Owner;
    if(Owner)WorldUIStateHandle=Owner->OnStateChanged().AddUObject(this,&ThisClass::HandleWorldUIState);
}
void UDivineBeastsRootLayout::HandleWorldUIState(const FDivineBeastsUIViewState&)
{ RefreshWorldContextHUD(); }
void UDivineBeastsRootLayout::ReleaseWorldContextHUD()
{
    // 先撤销根拥有的引用，再调用可同步取消命令的模型清理，后继选择不会被旧引用误清除。
    auto* Previous=ActiveWorldContextHUD.Get();ActiveWorldContextHUD.Reset();
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Model(WorldHUDModel);WorldHUDModel=nullptr;
    if(Previous){Previous->ClearWorldCommandContext();Previous->InitializeWidgetViewModel(nullptr);Previous->SetVisibility(ESlateVisibility::Collapsed);}
    if(Model.IsValid()){Model->OnScreenDeactivated();Model->EndPage();}
}
void UDivineBeastsRootLayout::RefreshWorldContextHUD()
{
    if(bSynchronizingWorldHUD){bWorldHUDRefreshQueued=true;return;}
    TGuardValue<bool> Guard(bSynchronizingWorldHUD,true);
    do{bWorldHUDRefreshQueued=false;RefreshWorldContextHUDOnce();}while(bWorldHUDRefreshQueued&&bWorldHUDHostConstructed);
}
void UDivineBeastsRootLayout::RefreshWorldContextHUDOnce()
{
    if(!bWorldHUDHostConstructed)return;
    auto* Switcher=Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("WorldContextSwitcher")));
    // Travel清理HUDLayer后只恢复本根布局拥有的既有Switcher实例，不构造新视觉树或复活旧世界动态HUD。
    if(Switcher&&!Switcher->GetParent()&&HUDLayer)
        if(auto* ContextSlot=HUDLayer->AddChildToOverlay(Switcher)){ContextSlot->SetHorizontalAlignment(HAlign_Left);ContextSlot->SetVerticalAlignment(VAlign_Center);}
    auto* Owner=WorldUIOwner.Get();
    const auto State=Owner?Owner->GetViewState():FDivineBeastsUIViewState();
    FName Role;
    const APawn* Pawn=GetOwningPlayerPawn();
    const bool bWorldVisible=Owner&&State.CurrentStep==TEXT("DBA.Flow.InWorld")&&!State.World.WorldId.IsNone()&&
        IsValid(Pawn)&&Pawn->IsLocallyControlled()&&FDivineBeastsProjectCatalog::TryGetServerRoleForExperience(State.World.ExperienceId,Role)&&
        FDivineBeastsWorldHUDRoute::IsSupportedWorldRole(Role);
    if(!Switcher||!bWorldVisible){ReleaseWorldContextHUD();if(Switcher)Switcher->SetVisibility(ESlateVisibility::Collapsed);return;}
    const int32 Index=FDivineBeastsWorldHUDRoute::ResolveSwitcherIndex(Role,State.World.ExperienceId);
    auto* Selected=Cast<UDivineBeastsWorldHUDBase>(Switcher->GetWidgetAtIndex(Index));
    if(!Selected){ReleaseWorldContextHUD();Switcher->SetVisibility(ESlateVisibility::Collapsed);return;}
    if(Selected!=ActiveWorldContextHUD.Get())
    {
        ReleaseWorldContextHUD();ActiveWorldContextHUD=Selected;
        if(!bWorldHUDHostConstructed||WorldUIOwner.Get()!=Owner){ActiveWorldContextHUD.Reset();return;}
        Selected->SetPlayerContext(FLocalPlayerContext(GetOwningPlayer()));
        WorldHUDModel=Owner->CreateViewModel(Selected->GetHUDSurfaceId());
        if(WorldHUDModel){WorldHUDModel->BeginPage();Selected->InitializeWidgetViewModel(WorldHUDModel);}
    }
    Switcher->SetActiveWidgetIndex(Index);Switcher->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Selected->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UDivineBeastsRootLayout::RefreshCombatHUDVisibility()
{
    if (!IsValid(CombatHUD))
    {
        return;
    }

    // 平台PrepareForTravel会清空HUDLayer；作者组合由项目Root持有，不等同于上一世界临时HUD。
    // 仅恢复自己的已保存实例，不偷取挂到其他面板的控件，也不复活被清理的动态控件。
    if (!CombatHUD->GetParent() && IsValid(HUDLayer))
    {
        if (UOverlaySlot* HUDSlot = HUDLayer->AddChildToOverlay(CombatHUD))
        {
            HUDSlot->SetHorizontalAlignment(HAlign_Fill);
            HUDSlot->SetVerticalAlignment(VAlign_Fill);
        }
    }

    const APawn* CurrentPawn = GetOwningPlayerPawn();
    const bool bHasGameplayAvatar =
        IsValid(CurrentPawn) && CurrentPawn->IsLocallyControlled() &&
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
    BindCastDisplay(DisplayPawn);
    BindWorldHealthBars(DisplayPawn);
    BindWorldCombatFeedback(DisplayPawn);
    for (FName PortraitName : {FName(TEXT("PlayerPortrait")), FName(TEXT("ActionBarPortrait"))})
    {
        if (auto* Portrait = Cast<UDivineBeastsPlayerPortraitWidget>(CombatHUD->GetWidgetFromName(PortraitName)))
        {
            Portrait->SetOwningPlayer(GetOwningPlayer());
            Portrait->BindToPawn(DisplayPawn);
        }
    }
    if (auto* Minimap = Cast<UDivineBeastsVillageMinimapWidget>(CombatHUD->GetWidgetFromName(TEXT("Minimap"))))
    {
        Minimap->SetOwningPlayer(GetOwningPlayer());
        Minimap->BindToPawn(DisplayPawn);
    }
    // 保留的根布局不一定再次NativeConstruct；当前Pawn事件统一重绑真实技能/属性来源。
    if (auto* Abilities = Cast<UDivineBeastsAbilityBarPanel>(CombatHUD->GetWidgetFromName(TEXT("AbilityBar"))))
    { Abilities->RefreshAbilitySourceFromOwningPawn(); }
    // 两处资源条都只读同一ASC事实；各自委托与异步句柄按实例配对释放，不复制游戏权威数据。
    for (FName StatusName : {FName(TEXT("PlayerStatus")), FName(TEXT("ActionBarStatus"))})
    {
        if (auto* Status = Cast<UDivineBeastsPlayerStatusPanel>(CombatHUD->GetWidgetFromName(StatusName)))
        { Status->RefreshStatusSourceFromOwningPawn(); }
    }
}

void UDivineBeastsRootLayout::UnbindCastDisplay()
{
    // 先撤销账本再进入Widget/蓝图回调，清理旧来源时同步产生的新绑定可以完整存活。
    auto* PreviousComponent = BoundCastComponent.Get();
    auto* PreviousIdentity = BoundCastIdentity.Get();
    auto* PreviousPawn = BoundCastPawn.Get();
    auto* PreviousWidget = BoundCastWidget.Get();
    BoundCastComponent.Reset(); BoundCastIdentity.Reset(); BoundCastPawn.Reset(); BoundCastWidget.Reset();
    CastDisplayScopeId.Invalidate(); CastDisplayRevision = 0;
    const FDelegateHandle PreviousFactsHandle = CastFactsHandle;
    const FDelegateHandle PreviousIdentityHandle = CastIdentityHandle;
    CastFactsHandle.Reset(); CastIdentityHandle.Reset();
    if (PreviousComponent) PreviousComponent->OnCastFactsChanged().Remove(PreviousFactsHandle);
    if (PreviousIdentity) PreviousIdentity->OnIdentityChanged().Remove(PreviousIdentityHandle);
    if (PreviousPawn) PreviousPawn->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleCastPawnEndPlay);
    if (PreviousWidget) PreviousWidget->ClearCastSource();
}

void UDivineBeastsRootLayout::BindCastDisplay(APawn* Pawn)
{
    auto* Component = IsValid(Pawn) ? Pawn->FindComponentByClass<UGamePlatformAbilityCastComponent>() : nullptr;
    auto* Widget = IsValid(CombatHUD) ? Cast<UGamePlatformCastProgressWidget>(CombatHUD->GetWidgetFromName(TEXT("CastBar"))) : nullptr;
    if (BoundCastPawn.Get() == Pawn && BoundCastComponent.Get() == Component &&
        BoundCastWidget.Get() == Widget && CastDisplayScopeId.IsValid() &&
        Widget && Widget->GetCastSnapshot().SourceScopeId == CastDisplayScopeId) return;
    UnbindCastDisplay();
    // 解绑可能同步建立后继来源；不能继续使用旧调用栈传入的Pawn。
    if (!bWorldHUDHostConstructed || CastDisplayScopeId.IsValid() || !IsValid(Pawn) || Pawn != GetOwningPlayerPawn() ||
        !Pawn->IsLocallyControlled() || Pawn->IsActorBeingDestroyed() || !IsValid(Component) || !IsValid(Widget)) return;
    const FGuid Scope = FGuid::NewGuid();
    CastDisplayScopeId = Scope; BoundCastPawn = Pawn; BoundCastComponent = Component; BoundCastWidget = Widget;
    auto* Identity = Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>();
    BoundCastIdentity = Identity;
    CastFactsHandle = Component->OnCastFactsChanged().AddUObject(this, &ThisClass::HandleCastFactsChanged);
    if (Identity) CastIdentityHandle = Identity->OnIdentityChanged().AddUObject(this, &ThisClass::RefreshCastDisplayFromIdentity);
    Pawn->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleCastPawnEndPlay);
    Widget->BindCastSource(Scope);
    if (CastDisplayScopeId != Scope || BoundCastComponent.Get() != Component) return;
    // 先订阅再读取快照，覆盖页面重显/绑定前已开始的动作；之后只依赖真实变化通知。
    HandleCastFactsChanged(Component, Component->GetCastFacts());
}

void UDivineBeastsRootLayout::RefreshCastDisplayFromIdentity()
{
    if (auto* Component = BoundCastComponent.Get()) HandleCastFactsChanged(Component, Component->GetCastFacts());
}

void UDivineBeastsRootLayout::HandleCastPawnEndPlay(AActor* Actor, EEndPlayReason::Type)
{
    if (Actor == BoundCastPawn.Get()) UnbindCastDisplay();
}

void UDivineBeastsRootLayout::HandleCastFactsChanged(UGamePlatformAbilityCastComponent* Component,
    const FGamePlatformAbilityCastFacts& Incoming)
{
    auto* Pawn = BoundCastPawn.Get();
    auto* Widget = BoundCastWidget.Get();
    if (!bWorldHUDHostConstructed || !CastDisplayScopeId.IsValid() || !IsValid(Component) || Component != BoundCastComponent.Get() ||
        !IsValid(Pawn) || Pawn != GetOwningPlayerPawn() || !Pawn->IsLocallyControlled() ||
        Pawn->IsActorBeingDestroyed() || Component->GetOwner() != Pawn || !IsValid(Widget)) return;
    const auto Facts = Component->GetCastFacts();
    if (Incoming.ActivationId != Facts.ActivationId || Incoming.AbilityId != Facts.AbilityId ||
        Incoming.PresentationStyleId != Facts.PresentationStyleId || Incoming.AvatarGeneration != Facts.AvatarGeneration ||
        Incoming.StartedServerSeconds != Facts.StartedServerSeconds || Incoming.DurationSeconds != Facts.DurationSeconds ||
        Incoming.bActive != Facts.bActive) return;
    // 从未发布时没有合法施法身份，保留已绑定的空条，不能制造假技能。
    if (!Facts.ActivationId.IsValid()) return;
    // 表现样式只决定显示名，不是服务器已确认施法的资格白名单。
    FText Name = NSLOCTEXT("DivineBeastsUI", "ConfirmedCast", "施法中");
    if (Facts.PresentationStyleId == TEXT("DBA.Development.Fireball")) Name = FText::FromString(TEXT("火焰弹 · 施法动作"));
    else if (Facts.PresentationStyleId == TEXT("DBA.Development.Frostbolt")) Name = FText::FromString(TEXT("寒冰箭 · 施法动作"));
    else if (Facts.PresentationStyleId == TEXT("DBA.Development.ArcaneMissiles")) Name = FText::FromString(TEXT("奥术飞弹 · 施法动作"));
    else if (Facts.PresentationStyleId == TEXT("DBA.Development.Healing")) Name = FText::FromString(TEXT("治疗 · 施法动作"));
    auto* Combat = Pawn->FindComponentByClass<UGamePlatformCombatComponent>();
    auto* Identity = BoundCastIdentity.Get();
    const int32 AvatarGeneration = Identity ? Identity->GetAvatarGeneration() : Combat ? Combat->GetCombatAvatarGeneration() : 0;
    const auto* World = Pawn->GetWorld();
    if (!World || World->bIsTearingDown) { UnbindCastDisplay(); return; }
    const auto* GameState = World->GetGameState();
    const double ServerSeconds = GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
    const bool bValidWindow = FMath::IsFinite(ServerSeconds) && FMath::IsFinite(Facts.StartedServerSeconds) &&
        FMath::IsFinite(Facts.DurationSeconds) && Facts.DurationSeconds > 0 && Facts.DurationSeconds <= 10;
    FGamePlatformUICastProgressState Snapshot;
    Snapshot.SourceScopeId = CastDisplayScopeId;
    Snapshot.CastInstanceId = FName(*Facts.ActivationId.ToString(EGuidFormats::Digits));
    Snapshot.CasterDisplayId = Pawn->GetFName(); Snapshot.DisplayName = Name;
    Snapshot.TotalSeconds = bValidWindow ? Facts.DurationSeconds : 0;
    Snapshot.RemainingSeconds = bValidWindow
        ? FMath::Clamp(Snapshot.TotalSeconds - FMath::Max(0.0, ServerSeconds - Facts.StartedServerSeconds), 0.0, Snapshot.TotalSeconds) : 0;
    Snapshot.bActive = Facts.bActive && bValidWindow && Snapshot.RemainingSeconds > 0 &&
        Facts.AvatarGeneration == AvatarGeneration && Combat &&
        !(Combat->GetCombatAvatarGeneration() == AvatarGeneration && Combat->IsCombatDead());
    // 显示服务器给出的真实窗口；UI不决定起手、释放时机、结束或可打断资格。
    Snapshot.bChanneling = false; Snapshot.bInterruptible = false;
    Snapshot.Revision = ++CastDisplayRevision;
    Widget->ApplyCastSnapshot(Snapshot);
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




void UDivineBeastsRootLayout::BindWorldHealthBars(APawn* DisplayPawn)
{
    auto* Controller = DisplayPawn ? Cast<ADivineBeastsWorldPlayerController>(GetOwningPlayer()) : nullptr;
    if (auto* Previous = Cast<ADivineBeastsWorldPlayerController>(WorldHealthController.Get()); Previous != Controller)
    {
        if (Previous)
        {
            Previous->OnFinalCameraViewChanged.RemoveAll(this);
            Previous->OnWorldHealthBarsToggleRequested.RemoveAll(this);
        }
        WorldHealthController = Controller;
        if (Controller)
        {
            Controller->OnFinalCameraViewChanged.AddUObject(this, &ThisClass::HandleFinalCameraViewChanged);
            Controller->OnWorldHealthBarsToggleRequested.AddUObject(this, &ThisClass::ToggleWorldHealthBars);
        }
    }
    if (DisplayPawn && WorldHealthBarClass)
    {
        if (!WorldHealthBars) WorldHealthBars = NewObject<UDivineBeastsWorldHealthBarPresenter>(this);
        WorldHealthBars->Initialize(GetOwningLocalPlayer(), DisplayPawn->GetWorld(), WorldHealthBarClass, SelectionRingMaterial);
        WorldHealthBars->SetVisible(bWorldHealthBarsVisible);
    }
    else if (WorldHealthBars) WorldHealthBars->Clear();
    if (CombatHUD)
    {
        if (auto* Toggle = Cast<UButton>(CombatHUD->GetWidgetFromName(TEXT("WorldHealthBarsToggle"))))
        {
            Toggle->OnClicked.RemoveDynamic(this, &ThisClass::ToggleWorldHealthBars);
            if (DisplayPawn) Toggle->OnClicked.AddUniqueDynamic(this, &ThisClass::ToggleWorldHealthBars);
        }
    }
    RefreshWorldHealthBarToggleText();
}

void UDivineBeastsRootLayout::UnbindWorldCombatFeedback()
{
    if (auto* Bus = BoundCombatFeedbackBus.Get())
    {
        if (CombatFeedbackHandle.IsValid()) Bus->OnConfirmedFeedback().Remove(CombatFeedbackHandle);
    }
    CombatFeedbackHandle.Reset();
    BoundCombatFeedbackBus.Reset();
}

void UDivineBeastsRootLayout::BindWorldCombatFeedback(APawn* DisplayPawn)
{
    // 只有普通世界控制器拥有此订阅；竞技场使用自己的组合根，不能重复显示同一事实。
    UWorld* World = IsValid(DisplayPawn) ? DisplayPawn->GetWorld() : nullptr;
    const bool bWorldReady = World && !World->bIsTearingDown &&
        World->GetNetMode() != NM_DedicatedServer &&
        IsValid(Cast<ADivineBeastsWorldPlayerController>(GetOwningPlayer())) &&
        IsValid(GetOwningLocalPlayer()); // 红屏与飘字独立：未配置飘字控件也必须接收受击事件。
    auto* Bus = bWorldReady ? World->GetSubsystem<UGamePlatformCombatFeedbackWorldSubsystem>() : nullptr;
    if (BoundCombatFeedbackBus.Get() == Bus) return;
    UnbindWorldCombatFeedback();
    if (Bus)
    {
        BoundCombatFeedbackBus = Bus;
        CombatFeedbackHandle = Bus->OnConfirmedFeedback().AddUObject(
            this, &ThisClass::HandleConfirmedWorldCombatFeedback);
    }
}

bool UDivineBeastsRootLayout::TryBuildLocalDamageFeedback(
    const FGamePlatformCombatEvent& Event, const AActor* LocalPawn, float MaxHealth, float& OutDamageFraction)
{
    OutDamageFraction = 0.0f;
    if (!IsValid(LocalPawn) || Event.TargetActor.Get() != LocalPawn || !Event.EventId.IsValid() ||
        Event.EventType != EGamePlatformCombatEventType::Damage ||
        !FMath::IsFinite(Event.AppliedToHealth) || Event.AppliedToHealth <= 0.0f ||
        !FMath::IsFinite(MaxHealth) || MaxHealth <= 0.0f) return false;
    // 强度使用本地可信最大生命进行归一化，客户端只作视觉提示，绝不回写Gameplay。
    OutDamageFraction = FMath::Clamp(Event.AppliedToHealth / MaxHealth, 0.0f, 1.0f);
    return OutDamageFraction > 0.0f;
}

void UDivineBeastsRootLayout::HandleConfirmedWorldCombatFeedback(const FGamePlatformCombatEvent& Event)
{
    ULocalPlayer* Player = GetOwningLocalPlayer();
    UWorld* World = GetWorld();
    AActor* Target = Event.TargetActor;
    if (!IsInGameThread() || !Player || !World || World->bIsTearingDown ||
        Player->GetWorld() != World || !BoundCombatFeedbackBus.IsValid() ||
        !Event.EventId.IsValid() || !IsValid(Target) ||
        Target->GetWorld() != World || Target->IsActorBeingDestroyed()) return;

    // 单一权威网络事实仅影响自己的本地视口；远端玩家受击与仅护盾吸收均不得触发红屏。
    if (Target == GetOwningPlayerPawn())
    {
        const auto* Combat = Target->FindComponentByClass<UGamePlatformCombatComponent>();
        float DamageFraction = 0.0f;
        if (IsValid(Combat) && TryBuildLocalDamageFeedback(Event, Target,
            Combat->GetCombatMaxHealth(), DamageFraction))
            BP_OnLocalPlayerDamaged(DamageFraction);
    }
    // 飘字是独立可选表现，不再决定本地受击视觉通知能否运行。
    if (!CombatFloatingTextClass || CombatFloatingTextClass->HasAnyClassFlags(CLASS_Abstract)) return;

    FVector Origin, Extent;
    Target->GetActorBounds(true, Origin, Extent);
    FDivineBeastsCombatFeedbackInput Input;
    Input.EventId = Event.EventId;
    Input.TargetVisualKey = Target->GetFName();
    // 近战没有可靠命中点时原事件坐标可为世界原点；飘字始终锚定目标头部。
    Input.WorldLocation = Origin + FVector(0.0, 0.0, FMath::Max(Extent.Z * 0.75, 60.0));
    auto Submit = [Player, this, &Input](EDivineBeastsCombatFeedbackKind Kind, double Magnitude, uint32 Salt)
    {
        if (!FMath::IsFinite(Magnitude) || Magnitude < 0.0) return;
        Input.Kind = Kind;
        Input.Magnitude = Magnitude;
        Input.EventId.A ^= Salt;
        UDivineBeastsCombatUIFeedbackLibrary::SubmitFloatingText(Player, Input, CombatFloatingTextClass);
        Input.EventId.A ^= Salt;
    };
    switch (Event.EventType)
    {
    case EGamePlatformCombatEventType::Damage:
        if (Event.AppliedToHealth > 0.0f) Submit(EDivineBeastsCombatFeedbackKind::Damage, Event.AppliedToHealth, 0u);
        if (Event.AppliedToShield > 0.0f) Submit(EDivineBeastsCombatFeedbackKind::ShieldDamage, Event.AppliedToShield, 0x53484C44u);
        break;
    case EGamePlatformCombatEventType::Healing:
        if (Event.AppliedMagnitude > 0.0f) Submit(EDivineBeastsCombatFeedbackKind::Healing, Event.AppliedMagnitude, 0x4845414Cu);
        break;
    case EGamePlatformCombatEventType::Death:
        Submit(EDivineBeastsCombatFeedbackKind::Death, 0.0, 0x44454154u);
        break;
    default:
        break;
    }
}

void UDivineBeastsRootLayout::ToggleWorldHealthBars()
{
    if (!GetOwningPlayerPawn() || !WorldHealthBars) return;
    bWorldHealthBarsVisible = !bWorldHealthBarsVisible;
    WorldHealthBars->SetVisible(bWorldHealthBarsVisible);
    RefreshWorldHealthBarToggleText();
}

void UDivineBeastsRootLayout::HandleFinalCameraViewChanged()
{
    if (WorldHealthBars) WorldHealthBars->RefreshProjection();
}

void UDivineBeastsRootLayout::RefreshWorldHealthBarToggleText()
{
    if (!CombatHUD) return;
    if (auto* Text = Cast<UTextBlock>(CombatHUD->GetWidgetFromName(TEXT("WorldHealthBarsToggleText"))))
        Text->SetText(FText::FromString(bWorldHealthBarsVisible ? TEXT("血条：显示 [V]") : TEXT("血条：隐藏 [V]")));
}

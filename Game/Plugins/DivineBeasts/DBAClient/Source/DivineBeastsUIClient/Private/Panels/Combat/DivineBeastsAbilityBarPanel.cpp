// 项目客户端技能HUD适配：消费当前Pawn的只读授权/冷却事件，不发送业务HTTP或授予技能。
// Widget独占ViewModel与订阅句柄；每次构造恢复订阅、失活解绑，允许同一Widget跨显示周期复用。
// 项目按键提示只读取当前玩家原生输入映射并更新已有HotkeyText；平台槽位不解释DBA动作名。
#include "Panels/Combat/DivineBeastsAbilityBarPanel.h"
#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "ViewModels/Combat/DivineBeastsAbilityBarViewModel.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/InputSettings.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"

void UDivineBeastsAbilityBarPanel::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshAbilitySourceFromOwningPawn();
    // 初始无GAS授权时清空真实Widget；新版最多八个可装载主动槽，旧版被动兼容。
    RefreshVisualSlots();
    BindAbilitySlotCommands(true);
}

void UDivineBeastsAbilityBarPanel::NativeDestruct()
{
    BindAbilitySlotCommands(false);
    VisualProjectionToken=FGuid::NewGuid();
    if (APlayerController* Controller = BoundPawnController.Get())
    {
        Controller->OnPossessedPawnChanged.RemoveDynamic(
            this, &UDivineBeastsAbilityBarPanel::HandlePossessedPawnChanged);
    }
    BoundPawnController.Reset();
    if (AbilityBarViewModel)
    {
        AbilityBarViewModel->OnSlotsChanged().Remove(ViewModelSlotsHandle);
        AbilityBarViewModel->UnbindFromLoadout();
    }
    ViewModelSlotsHandle.Reset();
    Super::NativeDestruct();
}

void UDivineBeastsAbilityBarPanel::BindAbilitySlotCommands(bool bBind)
{
    UGamePlatformSlotWidget* ActiveSlots[] =
    {
        PrimaryAbilitySlot.Get(), ActiveAbilitySlot1.Get(), ActiveAbilitySlot2.Get(),
        UltimateAbilitySlot.Get(), MovementAbilitySlot.Get(), DefenseAbilitySlot.Get(),
        ControlAbilitySlot.Get(), SignatureAbilitySlot.Get()
    };
    for (UGamePlatformSlotWidget* AbilitySlot : ActiveSlots)
    {
        if (!IsValid(AbilitySlot)) continue;
        AbilitySlot->OnSlotInvoked().RemoveAll(this);
        if (bBind) AbilitySlot->OnSlotInvoked().AddUObject(this, &ThisClass::HandleAbilitySlotInvoked);
    }
}

void UDivineBeastsAbilityBarPanel::HandleAbilitySlotInvoked(FName SlotId)
{
    if (!AbilityBarViewModel) return;
    const FGamePlatformResult Result = AbilityBarViewModel->RequestActivateSlot(SlotId);
    if (!Result.IsSuccess())
        UE_LOG(LogTemp, Warning, TEXT("技能输入请求被拒绝：Slot=%s Code=%s Reason=%s"),
            *SlotId.ToString(), *Result.Code.ToString(), *Result.Message);
}

void UDivineBeastsAbilityBarPanel::HandlePossessedPawnChanged(
    APawn* PreviousPawn, APawn* NewPawn)
{
    // 无论新旧角色是否同名都必须按当前 Controller 获取授权数据源。
    RefreshAbilitySourceFromOwningPawn();
}

bool UDivineBeastsAbilityBarPanel::RefreshAbilitySourceFromOwningPawn()
{
    APlayerController* Controller = GetOwningPlayer();
    if (APlayerController* Previous = BoundPawnController.Get(); Previous && Previous != Controller)
    { Previous->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged); }
    BoundPawnController = Controller;
    if (IsValid(Controller))
    { Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePossessedPawnChanged); }
    if (!AbilityBarViewModel)
    {
        AbilityBarViewModel = NewObject<UDivineBeastsAbilityBarViewModel>(this);
    }
    // RemoveFromParent后ViewModel仍被Widget拥有，NativeDestruct已撤销句柄；
    // 订阅生命周期必须独立于对象创建，否则再次显示后异步授权/冷却不再进入真实槽位。
    if (!ViewModelSlotsHandle.IsValid())
    {
        ViewModelSlotsHandle = AbilityBarViewModel->OnSlotsChanged().AddUObject(
            this, &UDivineBeastsAbilityBarPanel::HandleViewModelSlotsChanged);
    }
    APawn* Pawn = GetOwningPlayerPawn();
    UDivineBeastsAbilityLoadoutComponent* Loadout = Pawn
        ? Pawn->FindComponentByClass<UDivineBeastsAbilityLoadoutComponent>()
        : nullptr;
    const bool bBound = AbilityBarViewModel->BindToLoadout(Loadout);
    ApplyAbilitySlots(AbilityBarViewModel->GetSlotsRef());
    return bBound;
}

TArray<FDivineBeastsAbilitySlotDetails> UDivineBeastsAbilityBarPanel::GetAbilitySlotDetails() const
{
    // 蓝图仅消费与当前玩家绑定的只读提示，不得构造本地假技能。
    return AbilityBarViewModel ? AbilityBarViewModel->GetSlotDetails()
        : TArray<FDivineBeastsAbilitySlotDetails>();
}

void UDivineBeastsAbilityBarPanel::HandleViewModelSlotsChanged(
    const TArray<FGamePlatformUISlotState>& NewSlots)
{
    const int32 PreviousPresentationRevision = GetPresentationRevision();
    ApplyAbilitySlots(NewSlots);
    if (GetPresentationRevision() == PreviousPresentationRevision)
    {
        // ViewModel 只有槽位或 Tooltip 详细数据真实变化时才广播；
        // 即便槽位图标和禁用状态不变，技能名称、冷却剩余时间和说明仍须通知蓝图刷新。
        NotifyCombatPresentationChanged();
        BP_OnAbilitySlotsChanged();
    }
}


FText UDivineBeastsAbilityBarPanel::FormatHotkeyMappings(
    const TArray<FInputActionKeyMapping>& Mappings, bool bPassive)
{
    if (bPassive) return FText::GetEmpty();
    TArray<FString> Labels;
    for (const FInputActionKeyMapping& Mapping : Mappings)
    {
        // 本提示属于PC键鼠技能栏，手柄/触摸的设备提示由其输入界面拥有，不能冒充键盘快捷键。
        if (!Mapping.Key.IsValid() || Mapping.Key.IsGamepadKey() || Mapping.Key.IsTouch()) continue;
        FString Label;
        if (Mapping.bCtrl) Label += TEXT("Ctrl+");
        if (Mapping.bAlt) Label += TEXT("Alt+");
        if (Mapping.bShift) Label += TEXT("Shift+");
        if (Mapping.bCmd) Label += TEXT("Cmd+");
        Label += Mapping.Key.GetDisplayName().ToString();
        Labels.AddUnique(Label);
    }
    return FText::FromString(FString::Join(Labels, TEXT(" / ")));
}

void UDivineBeastsAbilityBarPanel::RefreshSlotHotkey(
    UGamePlatformSlotWidget* SlotWidget, FName ActionName, bool bPassive)
{
    if (!SlotWidget) return;
    auto* Hotkey = Cast<UTextBlock>(SlotWidget->GetWidgetFromName(TEXT("HotkeyText")));
    if (!Hotkey) return; // 旧资产缺少该可选显示原子时保持兼容；禁止动态补建控件树。
    TArray<FInputActionKeyMapping> Mappings;
    if (!bPassive && !ActionName.IsNone())
    {
        const APlayerController* Controller = GetOwningPlayer();
        if (Controller && Controller->PlayerInput)
            Mappings = Controller->PlayerInput->GetKeysForAction(ActionName);
        else if (const UInputSettings* Settings = GetDefault<UInputSettings>())
            Settings->GetActionMappingByName(ActionName, Mappings);
    }
    const FText Label = FormatHotkeyMappings(Mappings, bPassive);
    Hotkey->SetText(Label);
    // 仅固定已授权的提示字号与非命中策略；保留资产字体、颜色、位置、主题等视觉所有权。
    FSlateFontInfo HotkeyFont = Hotkey->GetFont();
    HotkeyFont.Size = 12;
    Hotkey->SetFont(HotkeyFont);
    Hotkey->SetVisibility(Label.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UDivineBeastsAbilityBarPanel::RefreshVisualSlots()
{
    const FGuid ProjectionToken=FGuid::NewGuid(); VisualProjectionToken=ProjectionToken;
    // 向八个可点击槽位投影服务器OwnerOnly授权，顺序与实体按键1—8保持一致。
    // Legacy被动仍供旧五槽资产显示，新版从单独Hero Trait区呈现。
    struct FVisualBinding
    {
        /** 作者资产中已经存在的槽控件；项目不创建或取得其资产所有权。 */
        UGamePlatformSlotWidget* Widget;
        /** 授权快照中的精确技能输入标签；被动使用稳定前缀。 */
        FName ExpectedInputSlotId;
        /** 引擎实际原生动作名，只用于只读查询；被动为空。 */
        FName ActionName;
        /** 被动不具有玩家激活快捷键，必须无条件隐藏按键提示。 */
        bool bPassive;
    };
    const FVisualBinding Bindings[] =
    {
        { PrimaryAbilitySlot.Get(),
            FName(TEXT("Platform.Ability.Input.DivineBeasts.Primary")), TEXT("DBA.PrimaryAbility"), false },
        { PassiveAbilitySlot.Get(), FName(TEXT("Ability.Passive")), NAME_None, true },
        { ActiveAbilitySlot1.Get(),
            FName(TEXT("Platform.Ability.Input.DivineBeasts.Slot1")), TEXT("DBA.AbilityOne"), false },
        { ActiveAbilitySlot2.Get(),
            FName(TEXT("Platform.Ability.Input.DivineBeasts.Slot2")), TEXT("DBA.AbilityTwo"), false },
        { UltimateAbilitySlot.Get(),
            FName(TEXT("Platform.Ability.Input.DivineBeasts.Slot4")), TEXT("DBA.UltimateAbility"), false },
        { MovementAbilitySlot.Get(),
            FName(TEXT("Platform.Ability.Input.DivineBeasts.Slot3")), TEXT("DBA.AbilitySlot3"), false },
        { DefenseAbilitySlot.Get(),
            FName(TEXT("Platform.Ability.Input.DivineBeasts.Slot5")), TEXT("DBA.AbilitySlot5"), false },
        { ControlAbilitySlot.Get(),
            FName(TEXT("Platform.Ability.Input.DivineBeasts.Slot6")), TEXT("DBA.AbilitySlot6"), false },
        { SignatureAbilitySlot.Get(),
            FName(TEXT("Platform.Ability.Input.DivineBeasts.Slot7")), TEXT("DBA.AbilitySlot7"), false }
    };

    // 仅实际拥有八个不同主动GAS槽位时隐藏旧被动控件，
    // 新旧英雄切换后自动恢复兼容，不创建额外Widget，也不依赖客户端伪造数据。
    if (IsValid(PassiveAbilitySlot))
    {
        TSet<FName> DistinctActiveInputSlots;
        for (const FGamePlatformUISlotState& GrantedSlot : AbilitySlots)
        {
            const FString SlotTag = GrantedSlot.SlotId.ToString();
            if (SlotTag == TEXT("Platform.Ability.Input.DivineBeasts.Primary") ||
                SlotTag.StartsWith(TEXT("Platform.Ability.Input.DivineBeasts.Slot")))
                DistinctActiveInputSlots.Add(GrantedSlot.SlotId);
        }
        PassiveAbilitySlot->SetVisibility(
            DistinctActiveInputSlots.Num() == 8 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }

    // 详情只在状态事件中读取一次；按槽位与技能双重身份匹配，避免同技能多槽时串用等级/冷却。
    const TArray<FDivineBeastsAbilitySlotDetails> Details = GetAbilitySlotDetails();
    for (const FVisualBinding& Binding : Bindings)
    {
        if (!Binding.Widget)
        {
            // 可选控件允许既有Widget蓝图在不失效的情况下逐步更新。
            continue;
        }

        FGamePlatformUISlotState VisualState;
        VisualState.SlotId = Binding.ExpectedInputSlotId;
        VisualState.bEnabled = false;
        VisualState.bPending = false;

        bool bMatched = false;
        bool bAmbiguous = false;
        for (const FGamePlatformUISlotState& Granted : AbilitySlots)
        {
            const bool bMatches = Binding.bPassive
                ? Granted.SlotId.ToString().StartsWith(TEXT("Ability.Passive."))
                : Granted.SlotId == Binding.ExpectedInputSlotId;
            if (!bMatches)
            {
                continue;
            }
            if (bMatched)
            {
                // 重复被动槽不能按加载顺序挑选一个；明确降级为不可交互。
                bAmbiguous = true;
                break;
            }
            bMatched = true;
            VisualState = Granted;
        }
        if (bAmbiguous)
        {
            VisualState = FGamePlatformUISlotState();
            VisualState.SlotId = Binding.ExpectedInputSlotId;
            VisualState.bEnabled = false;
            VisualState.bPending = true;
        }
        Binding.Widget->ApplySlotState(VisualState);
        // BP_OnSlotStateChanged同步重入刷新或失活时，旧栈停止，不覆盖新只读快照的图标/冷却。
        if (VisualProjectionToken!=ProjectionToken || !IsValid(Binding.Widget)) return;
        // UIProfile/图标已经由ViewModel的中央Data租约预热。这里只取驻留对象，不同步加载，
        // 并取消旧蓝图SetBrushFromSoftTexture的自有在途请求；空引用必须主动清画刷，不能只隐藏。
        // 这样换Pawn/缺图标后的迟到旧纹理不再复活，也不把未授权槽补成固定魔法图标。
        if (auto* Icon = Cast<UImage>(Binding.Widget->GetWidgetFromName(TEXT("AbilityIcon"))))
        {
            UTexture2D* Texture = VisualState.Icon.Get();
            Icon->SetBrushFromTexture(Texture);
            Icon->SetVisibility(IsValid(Texture) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
            // 可用资格由槽位命令门禁负责；冷却期间图标保持可见原色，不以禁用Shader隐藏内容。
            Icon->SetIsEnabled(true);
            Icon->SetRenderOpacity(1.f);
        }
        if (auto* Cooldown = Cast<UProgressBar>(Binding.Widget->GetWidgetFromName(TEXT("CooldownFill"))))
            Cooldown->SetPercent(VisualState.OverlayProgress);
        const FDivineBeastsAbilitySlotDetails* Detail = Details.FindByPredicate(
            [&VisualState](const FDivineBeastsAbilitySlotDetails& Candidate)
            {
                return !VisualState.ContentId.IsNone() && Candidate.SlotId == VisualState.SlotId &&
                    Candidate.AbilityId == VisualState.ContentId;
            });
        FText Tooltip;
        if (Detail)
        {
            Tooltip = FText::Format(NSLOCTEXT("DBAAbilityBar", "SkillTitle", "{0} · 等级 {1}"),
                Detail->DisplayName, FText::AsNumber(Detail->AbilityLevel));
            if (!Detail->Description.IsEmpty())
                Tooltip = FText::Format(NSLOCTEXT("DBAAbilityBar", "DetailLine", "{0}\n{1}"), Tooltip, Detail->Description);
            // 被动无主动操作入口；不能把这一正常状态提示为“当前条件不允许使用”。
            const FText StateText = VisualState.bPending
                ? NSLOCTEXT("DBAAbilityBar", "Pending", "技能授权正在确认")
                : Binding.bPassive ? NSLOCTEXT("DBAAbilityBar", "Passive", "被动技能") : Detail->DisabledReason;
            if (!StateText.IsEmpty())
                Tooltip = FText::Format(NSLOCTEXT("DBAAbilityBar", "DetailLine", "{0}\n{1}"), Tooltip, StateText);
        }
        else if (!VisualState.ContentId.IsNone())
            Tooltip = NSLOCTEXT("DBAAbilityBar", "DetailsPending", "技能资料尚未加载");
        Binding.Widget->SetToolTipText(Tooltip);
        if (auto* Level = Cast<UTextBlock>(Binding.Widget->GetWidgetFromName(TEXT("LevelText"))))
        {
            Level->SetText(Detail && Detail->AbilityLevel > 0 ? FText::AsNumber(Detail->AbilityLevel) : FText::GetEmpty());
            Level->SetVisibility(Level->GetText().IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
        }
        if (auto* CooldownText = Cast<UTextBlock>(Binding.Widget->GetWidgetFromName(TEXT("CooldownText"))))
        {
            // 连续进度交给材质时钟；文字只标记冷却状态，避免把事件时刻的剩余秒数永久冻结在UI上。
            const bool bCooling = Detail && Detail->CooldownRemainingSeconds > 0.f && VisualState.OverlayProgress > 0.f;
            CooldownText->SetText(bCooling ? NSLOCTEXT("DBAAbilityBar", "Cooling", "冷却") : FText::GetEmpty());
            CooldownText->SetVisibility(bCooling ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
        }
        const float EndsAtAppSeconds = Detail ? Detail->CooldownEndsAtAppSeconds : 0.f;
        const float DurationSeconds = Detail ? Detail->CooldownTotalSeconds : 0.f;
        // 快照截止时间不会因其他刷新而重启动画；只更新材质参数，结束事件清除覆盖。
        Binding.Widget->ApplyOverlayTiming(EndsAtAppSeconds, DurationSeconds);
        // 在槽位蓝图状态事件之后刷新，不能让初始占位文字替代玩家当前生效按键。
        RefreshSlotHotkey(Binding.Widget, Binding.ActionName, Binding.bPassive);
    }
}

void UDivineBeastsAbilityBarPanel::ApplyAbilitySlots(
    const TArray<FGamePlatformUISlotState>& InSlots)
{
    bool bChanged = AbilitySlots.Num() != InSlots.Num();

    if (!bChanged)
    {
        for (int32 Index = 0; Index < InSlots.Num(); ++Index)
        {
            const FGamePlatformUISlotState& A = AbilitySlots[Index];
            const FGamePlatformUISlotState& B = InSlots[Index];

            if (A.SlotId != B.SlotId ||
                A.ContentId != B.ContentId ||
                A.Icon != B.Icon ||
                A.Count != B.Count ||
                !FMath::IsNearlyEqual(
                    A.OverlayProgress,
                    B.OverlayProgress,
                    0.0001f) ||
                A.bEnabled != B.bEnabled ||
                A.bPending != B.bPending)
            {
                bChanged = true;
                break;
            }
        }
    }

    if (!bChanged)
    {
        // 换Pawn/构造/快照事件即使技能投影相同，也重新读取当前玩家输入映射；无轮询或按键写入。
        RefreshVisualSlots();
        return;
    }

    AbilitySlots = InSlots;
    for (FGamePlatformUISlotState& AbilitySlotState : AbilitySlots)
    {
        AbilitySlotState.OverlayProgress =
            FMath::Clamp(
                AbilitySlotState.OverlayProgress,
                0.0f,
                1.0f);
    }
    // 同一只读快照驱动真实子Widget的图标、冷却遮罩与禁用反馈。
    RefreshVisualSlots();

    NotifyCombatPresentationChanged();
    BP_OnAbilitySlotsChanged();
}

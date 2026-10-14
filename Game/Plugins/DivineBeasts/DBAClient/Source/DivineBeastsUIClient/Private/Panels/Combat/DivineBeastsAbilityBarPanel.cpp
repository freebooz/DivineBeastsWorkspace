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
    // 初始没有技能授权时，也应清空并禁用真实可见的五个子槽位。
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
    UGamePlatformSlotWidget* ActiveSlots[] = {PrimaryAbilitySlot.Get(), ActiveAbilitySlot1.Get(),
        ActiveAbilitySlot2.Get(), UltimateAbilitySlot.Get()};
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
    // 当前项目只声明了普通攻击、被动、两个主动与终极技能槽，
    // 不允许用数组顺序或英雄名称隐式决定技能输入映射。
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
            FName(TEXT("Platform.Ability.Input.DivineBeasts.Slot4")), TEXT("DBA.UltimateAbility"), false }
    };

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
            Icon->SetIsEnabled(VisualState.bEnabled && !VisualState.bPending);
        }
        if (auto* Cooldown = Cast<UProgressBar>(Binding.Widget->GetWidgetFromName(TEXT("CooldownFill"))))
            Cooldown->SetPercent(VisualState.OverlayProgress);
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

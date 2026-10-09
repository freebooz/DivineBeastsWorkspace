#include "Panels/Combat/DivineBeastsAbilityBarPanel.h"
#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "ViewModels/Combat/DivineBeastsAbilityBarViewModel.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UDivineBeastsAbilityBarPanel::NativeConstruct()
{
    Super::NativeConstruct();
    if (APlayerController* Controller = GetOwningPlayer())
    {
        Controller->OnPossessedPawnChanged.AddUniqueDynamic(
            this, &UDivineBeastsAbilityBarPanel::HandlePossessedPawnChanged);
    }
    RefreshAbilitySourceFromOwningPawn();
    // 初始没有技能授权时，也应清空并禁用真实可见的五个子槽位。
    RefreshVisualSlots();
}

void UDivineBeastsAbilityBarPanel::NativeDestruct()
{
    if (APlayerController* Controller = GetOwningPlayer())
    {
        Controller->OnPossessedPawnChanged.RemoveDynamic(
            this, &UDivineBeastsAbilityBarPanel::HandlePossessedPawnChanged);
    }
    if (AbilityBarViewModel)
    {
        AbilityBarViewModel->OnSlotsChanged().Remove(ViewModelSlotsHandle);
        AbilityBarViewModel->UnbindFromLoadout();
    }
    ViewModelSlotsHandle.Reset();
    Super::NativeDestruct();
}

void UDivineBeastsAbilityBarPanel::HandlePossessedPawnChanged(
    APawn* PreviousPawn, APawn* NewPawn)
{
    // 无论新旧角色是否同名都必须按当前 Controller 获取授权数据源。
    RefreshAbilitySourceFromOwningPawn();
}

bool UDivineBeastsAbilityBarPanel::RefreshAbilitySourceFromOwningPawn()
{
    if (!AbilityBarViewModel)
    {
        AbilityBarViewModel = NewObject<UDivineBeastsAbilityBarViewModel>(this);
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


void UDivineBeastsAbilityBarPanel::RefreshVisualSlots()
{
    // 当前项目只声明了普通攻击、被动、两个主动与终极技能槽，
    // 不允许用数组顺序或英雄名称隐式决定技能输入映射。
    struct FVisualBinding
    {
        UGamePlatformSlotWidget* Widget;
        FName ExpectedInputSlotId;
        bool bPassive;
    };
    const FVisualBinding Bindings[] =
    {
        { PrimaryAbilitySlot.Get(),
            FName(TEXT("Platform.Ability.Input.DivineBeasts.Primary")), false },
        { PassiveAbilitySlot.Get(), FName(TEXT("Ability.Passive")), true },
        { ActiveAbilitySlot1.Get(),
            FName(TEXT("Platform.Ability.Input.DivineBeasts.Slot1")), false },
        { ActiveAbilitySlot2.Get(),
            FName(TEXT("Platform.Ability.Input.DivineBeasts.Slot2")), false },
        { UltimateAbilitySlot.Get(),
            FName(TEXT("Platform.Ability.Input.DivineBeasts.Slot4")), false }
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

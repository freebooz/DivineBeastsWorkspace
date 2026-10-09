#include "ViewModels/Combat/DivineBeastsAbilityBarViewModel.h"

#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "Definitions/DivineBeastsAbilityUIProfile.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

bool UDivineBeastsAbilityBarViewModel::BindToLoadout(
    UDivineBeastsAbilityLoadoutComponent* InLoadout)
{
    UnbindFromLoadout();
    if (!IsValid(InLoadout))
    {
        return false;
    }

    Loadout = InLoadout;
    LoadoutHandle = InLoadout->OnLoadoutChanged().AddUObject(
        this, &UDivineBeastsAbilityBarViewModel::HandleLoadoutChanged);
    HandleLoadoutChanged(InLoadout->GetLoadoutStateRef());
    return true;
}

void UDivineBeastsAbilityBarViewModel::UnbindFromLoadout()
{
    if (UDivineBeastsAbilityLoadoutComponent* Current = Loadout.Get())
    {
        Current->OnLoadoutChanged().Remove(LoadoutHandle);
    }
    LoadoutHandle.Reset();
    Loadout.Reset();
    ++LoadGeneration;
    ResetProfileLease();
    if (!Slots.IsEmpty())
    {
        Slots.Reset();
        MarkStateChanged();
        SlotsChanged.Broadcast(Slots);
    }
}

void UDivineBeastsAbilityBarViewModel::BeginDestroy()
{
    UnbindFromLoadout();
    Super::BeginDestroy();
}

void UDivineBeastsAbilityBarViewModel::ResetProfileLease()
{
    if (ProfileHandle.IsValid())
    {
        ProfileHandle->CancelHandle();
        ProfileHandle.Reset();
    }
    LoadedProfile = nullptr;
    LoadedHeroId = NAME_None;
}

void UDivineBeastsAbilityBarViewModel::HandleLoadoutChanged(
    const FDivineBeastsAbilityLoadoutState& Snapshot)
{
    if (!Loadout.IsValid())
    {
        return;
    }
    if (Snapshot.HeroDefinitionId != LoadedHeroId)
    {
        ++LoadGeneration;
        ResetProfileLease();
        LoadedHeroId = Snapshot.HeroDefinitionId;
        if (!LoadedHeroId.IsNone())
        {
            const FPrimaryAssetId ProfileId(
                FPrimaryAssetType(TEXT("DivineBeastsAbilityUIProfile")), LoadedHeroId);
            const int32 Serial = LoadGeneration;
            const FName Hero = LoadedHeroId;
            const FStreamableDelegate Completion = FStreamableDelegate::CreateUObject(
                this, &UDivineBeastsAbilityBarViewModel::HandleProfileLoaded, Serial, Hero);
            // 使用引擎统一 AssetManager（资产管理器）的已注册主资产，而不是猜测文件路径。
            // 缺失 Profile 时仍以真实服务器授权列表显示中性缺图标槽位。
            ProfileHandle = UAssetManager::Get().LoadPrimaryAsset(
                ProfileId, { FName(TEXT("UI")) }, Completion);
        }
    }
    RefreshSlots(Snapshot);
}

void UDivineBeastsAbilityBarViewModel::HandleProfileLoaded(
    int32 ExpectedLoadGeneration, FName ExpectedHero)
{
    if (ExpectedLoadGeneration != LoadGeneration ||
        ExpectedHero != LoadedHeroId || !Loadout.IsValid())
    {
        return;
    }
    const FPrimaryAssetId Id(
        FPrimaryAssetType(TEXT("DivineBeastsAbilityUIProfile")), ExpectedHero);
    UDivineBeastsAbilityUIProfile* Profile = Cast<UDivineBeastsAbilityUIProfile>(
        UAssetManager::Get().GetPrimaryAssetObject(Id));
    FString Error;
    if (Profile && Profile->HeroDefinitionId == ExpectedHero &&
        Profile->ValidateProfile(Error))
    {
        LoadedProfile = Profile;
    }
    else
    {
        LoadedProfile = nullptr;
    }
    RefreshSlots(Loadout->GetLoadoutStateRef());
}

void UDivineBeastsAbilityBarViewModel::RefreshSlots(
    const FDivineBeastsAbilityLoadoutState& Snapshot)
{
    TArray<FGamePlatformUISlotState> NewSlots;
    NewSlots.Reserve(Snapshot.Slots.Num());
    TSet<FName> SeenSlots;
    for (const FDivineBeastsGrantedAbilitySlot& Grant : Snapshot.Slots)
    {
        if (Grant.AbilityId.IsNone() || Grant.SlotId.IsNone() ||
            Grant.AbilityLevel < 1 || SeenSlots.Contains(Grant.SlotId))
        {
            // 异常网络投影 fail-closed；不能以重复或未知槽位覆盖真实授权。
            continue;
        }
        SeenSlots.Add(Grant.SlotId);
        FGamePlatformUISlotState Slot;
        Slot.SlotId = Grant.SlotId;
        Slot.ContentId = Grant.AbilityId;
        Slot.Count = INDEX_NONE;
        Slot.OverlayProgress = 0.0f;
        Slot.bEnabled = Snapshot.bReady && Grant.InputTag.IsValid();
        Slot.bPending = !Snapshot.bReady;
        if (LoadedProfile)
        {
            if (const FDivineBeastsAbilityUIEntry* UI = LoadedProfile->FindEntry(Grant.AbilityId))
            {
                Slot.Icon = UI->Icon;
            }
        }
        NewSlots.Add(MoveTemp(Slot));
    }

    bool bChanged = Slots.Num() != NewSlots.Num();
    if (!bChanged)
    {
        for (int32 Index = 0; Index < Slots.Num(); ++Index)
        {
            const FGamePlatformUISlotState& A = Slots[Index];
            const FGamePlatformUISlotState& B = NewSlots[Index];
            if (A.SlotId != B.SlotId || A.ContentId != B.ContentId ||
                A.Icon != B.Icon || A.bEnabled != B.bEnabled ||
                A.bPending != B.bPending)
            {
                bChanged = true;
                break;
            }
        }
    }
    if (bChanged)
    {
        Slots = MoveTemp(NewSlots);
        MarkStateChanged();
        SlotsChanged.Broadcast(Slots);
    }
}

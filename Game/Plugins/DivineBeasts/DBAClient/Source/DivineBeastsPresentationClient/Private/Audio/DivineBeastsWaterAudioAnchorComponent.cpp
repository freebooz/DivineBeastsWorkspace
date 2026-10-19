// SSLM水声的场景Actor适配器：真实组件位置→项目音频语义→平台SFX；无额外AudioComponent或Ticker。
#include "Audio/DivineBeastsWaterAudioAnchorComponent.h"

#include "DivineBeastsPresentationClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UDivineBeastsWaterAudioAnchorComponent::UDivineBeastsWaterAudioAnchorComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDivineBeastsWaterAudioAnchorComponent::BeginPlay()
{
    Super::BeginPlay();
    RefreshWaterAudio();
}

bool UDivineBeastsWaterAudioAnchorComponent::BindLocalPresentation()
{
    UWorld* World = GetWorld();
    if (!World || World->bIsTearingDown || !World->IsGameWorld() ||
        World->GetNetMode() == NM_DedicatedServer) return false;
    auto* GameInstance = World->GetGameInstance();
    ULocalPlayer* LocalPlayer = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
    if (!LocalPlayer || LocalPlayer->GetWorld() != World) return false;
    auto* Client = LocalPlayer->GetSubsystem<UDivineBeastsPresentationClientSubsystem>();
    if (!Client) return false;
    if (BoundPresentation.Get() != Client)
    {
        if (auto* Previous = BoundPresentation.Get())
            if (PackChangedHandle.IsValid())
                Previous->OnContentPackChanged().Remove(PackChangedHandle);
        PackChangedHandle = Client->OnContentPackChanged().AddUObject(
            this, &UDivineBeastsWaterAudioAnchorComponent::HandleContentPackChanged);
        BoundPresentation = Client;
    }
    BoundWorld = World;
    if (OwnedAnchorId.IsNone())
        OwnedAnchorId = FName(*GetPathName()); // 同一World内唯一、与场景对象生命周期绑定。
    return true;
}

bool UDivineBeastsWaterAudioAnchorComponent::RefreshWaterAudio()
{
    check(IsInGameThread());
    AActor* Source = GetOwner();
    UWorld* World = GetWorld();
    if (!IsValid(Source) || !World || Source->GetWorld() != World ||
        RequiredSourceTag.IsNone() || !Source->ActorHasTag(RequiredSourceTag) ||
        (WaterLoopClipId != TEXT("Ambience.WaterStreamA") &&
         WaterLoopClipId != TEXT("Ambience.WaterStreamB")) ||
        GetComponentLocation().ContainsNaN())
        return false;

    if (!BindLocalPresentation()) return false;
    UDivineBeastsPresentationClientSubsystem* Client = BoundPresentation.Get();
    if (!Client) return false;

    // 可安全重复申请：现有分类接口会复用Loading/Active租约，不产生第二个SFX执行器。
    FString Error;
    Client->ActivateAudioLibraryCategory(TEXT("Ambience"), Error);
    if (Client->GetAudioLibraryCategoryState(TEXT("Ambience")) !=
        EDivineBeastsPresentationContentPackState::Active)
        return false; // 尚未预载完成则等待内容包Active回调，不在这里轮询。
    return Client->StartAudioLibraryLoop(OwnedAnchorId, WaterLoopClipId, GetComponentLocation());
}

void UDivineBeastsWaterAudioAnchorComponent::HandleContentPackChanged(
    const FDivineBeastsPresentationContentPackHandle& Handle,
    EDivineBeastsPresentationContentPackState State, const FString& Error)
{
    if (State != EDivineBeastsPresentationContentPackState::Active) return;
    if (auto* Client = BoundPresentation.Get())
        if (Client->GetAudioLibraryCategoryState(TEXT("Ambience")) ==
            EDivineBeastsPresentationContentPackState::Active)
            RefreshWaterAudio(); // 同锚点重复Start由已有AudioLibraryLoops幂等处理。
}

void UDivineBeastsWaterAudioAnchorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 始终先退订再停止；世界正在卸载时由LocalPlayer世界清理兜底取消其余循环。
    if (auto* Client = BoundPresentation.Get())
    {
        if (PackChangedHandle.IsValid())
            Client->OnContentPackChanged().Remove(PackChangedHandle);
        if (auto* LocalPlayer = Client->GetLocalPlayer())
            if (LocalPlayer->GetWorld() == BoundWorld.Get() && !OwnedAnchorId.IsNone())
                Client->StopAudioLibraryLoop(OwnedAnchorId);
    }
    PackChangedHandle.Reset();
    BoundPresentation.Reset();
    BoundWorld.Reset();
    OwnedAnchorId = NAME_None;
    Super::EndPlay(EndPlayReason);
}

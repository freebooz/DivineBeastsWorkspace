// 神兽联盟客户端场景组件：Begin/EndPlay事件建立/撤销表现请求；不启动自己的渲染器。
#include "Environment/DivineBeastsBirdFlockSceneComponent.h"
#include "DivineBeastsPresentationClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UDivineBeastsBirdFlockSceneComponent::UDivineBeastsBirdFlockSceneComponent()
{
    PrimaryComponentTick.bCanEverTick=false;
    SetIsReplicatedByDefault(false);
}
TArray<FString> UDivineBeastsBirdFlockSceneComponent::GetBirdSpeciesOptions() const
{
    return {TEXT("Seagull"),TEXT("Swallow"),TEXT("Eagle")};
}
void UDivineBeastsBirdFlockSceneComponent::BeginPlay()
{
    Super::BeginPlay();
    FString Error;
    if(!ApplyBirdFlock(Error) && !Error.IsEmpty()) UE_LOG(LogTemp,Warning,TEXT("[BirdFlock] 场景锚点：%s"),*Error);
}
bool UDivineBeastsBirdFlockSceneComponent::ApplyBirdFlock(FString& OutError)
{
    OutError.Reset();
    UWorld* World=GetWorld();
    if(!World || !World->IsGameWorld() || World->GetNetMode()==NM_DedicatedServer || !GetOwner()) return false;
    UGameInstance* GI=World->GetGameInstance();
    ULocalPlayer* Player=GI?GI->GetFirstGamePlayer():nullptr;
    auto* Service=Player?Player->GetSubsystem<UDivineBeastsPresentationClientSubsystem>():nullptr;
    if(!Service) {OutError=TEXT("本地玩家表现服务尚未建立。");return false;}
    const FName NewId=FlockId.IsNone()?GetOwner()->GetFName():FlockId;
    if(BoundPresentation.IsValid() && (BoundPresentation.Get()!=Service || BoundFlockId!=NewId)) StopBirdFlock();
    if(!Service->StartAmbientBirdFlock(NewId,SpeciesId,BirdCount,GetComponentLocation(),OutError)) return false;
    BoundPresentation=Service; BoundFlockId=NewId; return true;
}
void UDivineBeastsBirdFlockSceneComponent::StopBirdFlock()
{
    if(auto* Service=BoundPresentation.Get()) Service->StopAmbientBirdFlock(BoundFlockId);
    BoundPresentation.Reset(); BoundFlockId=NAME_None;
}
void UDivineBeastsBirdFlockSceneComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    StopBirdFlock(); Super::EndPlay(Reason);
}
#if WITH_EDITOR
void UDivineBeastsBirdFlockSceneComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event);
    // PIE属性修改是一次用户事件，不引入Tick轮询；普通编辑器只配置不播放正式服务。
    if(GetWorld() && GetWorld()->IsGameWorld() && HasBegunPlay())
    {
        FString Error; ApplyBirdFlock(Error);
        if(!Error.IsEmpty()) UE_LOG(LogTemp,Warning,TEXT("[BirdFlock] 参数修改被拒绝：%s"),*Error);
    }
}
#endif

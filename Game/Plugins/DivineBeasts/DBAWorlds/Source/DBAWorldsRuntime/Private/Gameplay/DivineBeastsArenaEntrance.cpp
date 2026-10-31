#include "Gameplay/DivineBeastsArenaEntrance.h"
#include "Definitions/DivineBeastsArenaEntranceDefinition.h"
#include "Components/GamePlatformInteractableComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Net/UnrealNetwork.h"
#include "Types/GamePlatformInteractionSession.h"

ADivineBeastsArenaEntrance::ADivineBeastsArenaEntrance()
{
    PrimaryActorTick.bCanEverTick=false;
    bReplicates=true;
    SetReplicateMovement(false);
    auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("EntranceRoot"));
    Root->SetMobility(EComponentMobility::Static); SetRootComponent(Root);
    Frame=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame")); Frame->SetupAttachment(Root);
    Frame->SetMobility(EComponentMobility::Static); Frame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Frame->SetCanEverAffectNavigation(false);
    Surface=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surface")); Surface->SetupAttachment(Root);
    Surface->SetMobility(EComponentMobility::Static); Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Surface->SetCanEverAffectNavigation(false); Surface->SetCastShadow(false);
    Title=CreateDefaultSubobject<UTextRenderComponent>(TEXT("Title")); Title->SetupAttachment(Root);
    Title->SetRelativeLocation(FVector(-34,0,452)); Title->SetRelativeRotation(FRotator(0,180,0));
    Title->SetHorizontalAlignment(EHTA_Center); Title->SetWorldSize(44.f);
    Title->SetTextRenderColor(FColor(226,218,170)); Title->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Interactable=CreateDefaultSubobject<UGamePlatformInteractableComponent>(TEXT("ActivityInteraction"));
    Interactable->SetupAttachment(Root); Interactable->SetRelativeLocation(FVector(0,0,155));
    ClickSurface=CreateDefaultSubobject<UBoxComponent>(TEXT("ClickSurface")); ClickSurface->SetupAttachment(Root);
    ClickSurface->SetMobility(EComponentMobility::Static);
    ClickSurface->SetRelativeLocation(FVector(0,0,195)); ClickSurface->SetBoxExtent(FVector(18,150,195));
    ClickSurface->SetCollisionEnabled(ECollisionEnabled::QueryOnly); ClickSurface->SetCollisionResponseToAllChannels(ECR_Ignore);
    ClickSurface->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); ClickSurface->SetCanEverAffectNavigation(false);
    LeftPillar=CreateDefaultSubobject<UBoxComponent>(TEXT("LeftPillar")); LeftPillar->SetupAttachment(Root);
    RightPillar=CreateDefaultSubobject<UBoxComponent>(TEXT("RightPillar")); RightPillar->SetupAttachment(Root);
    for (auto* Pillar : {LeftPillar.Get(),RightPillar.Get()})
    { Pillar->SetMobility(EComponentMobility::Static); Pillar->SetBoxExtent(FVector(30,25,165)); Pillar->SetCollisionProfileName(TEXT("BlockAll")); }
    LeftPillar->SetRelativeLocation(FVector(0,-169,165)); RightPillar->SetRelativeLocation(FVector(0,169,165));
}
FName ADivineBeastsArenaEntrance::GetArenaModeId() const
{ return IsValid(EntranceDefinition)&&EntranceDefinition->IsConfigured()?EntranceDefinition->ArenaModeId:NAME_None; }
void ADivineBeastsArenaEntrance::RefreshDefinition()
{
    const FName Mode=GetArenaModeId();
    Title->SetText(IsValid(EntranceDefinition)?EntranceDefinition->DisplayName:FText::GetEmpty());
    if (!HasAuthority()) return;
    if (Mode.IsNone()) { Interactable->SetInteractionEnabled(false); return; }
    FGamePlatformInteractionOption Option;
    Option.OptionId=TEXT("Arena.Entry.Open"); Option.PromptId=Mode;
    Option.Mode=EGamePlatformInteractionMode::Instant; Option.MaxDistance=250.f; Option.bRequireLineOfSight=true;
    Option.ConcurrencyPolicy=EGamePlatformInteractionConcurrencyPolicy::Shared; Option.MaxConcurrent=32;
    Option.CommitKind=EGamePlatformInteractionCommitKind::Custom;
    if (Interactable->SetOptions({Option})) Interactable->SetInteractionEnabled(true);
}
void ADivineBeastsArenaEntrance::OnConstruction(const FTransform& Transform)
{ Super::OnConstruction(Transform); RefreshDefinition(); }
void ADivineBeastsArenaEntrance::BeginPlay()
{ RefreshDefinition(); Super::BeginPlay(); }
void ADivineBeastsArenaEntrance::OnRep_Definition() { RefreshDefinition(); }
void ADivineBeastsArenaEntrance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ADivineBeastsArenaEntrance,EntranceDefinition); }
bool ADivineBeastsArenaEntrance::CanBeginInteraction(const FGamePlatformInteractionSession& Session,const FGamePlatformInteractionOption& Option) const
{ return HasAuthority()&&!GetArenaModeId().IsNone()&&Session.TargetActor==this&&Option.OptionId==TEXT("Arena.Entry.Open"); }
bool ADivineBeastsArenaEntrance::CommitInteraction(const FGamePlatformInteractionSession& Session,const FGamePlatformInteractionOption& Option)
{ return CanBeginInteraction(Session,Option)&&Session.OptionId==Option.OptionId&&Session.RequestId.IsValid()&&Session.SessionId.IsValid(); }

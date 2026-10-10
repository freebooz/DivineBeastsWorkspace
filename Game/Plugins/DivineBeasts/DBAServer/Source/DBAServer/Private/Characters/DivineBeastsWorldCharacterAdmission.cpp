// 第三层专用服务器角色桥：PlayerData拥有持久资料，平台拥有出生/控制，本桥只读取并初始化既有角色。
// 无Tick/客户端RPC；失败、撤销、世界关闭和迟到响应都拒绝发布；响应及环境凭据不进入日志。
#include "Characters/DivineBeastsWorldCharacterAdmission.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Gameplay/DivineBeastsWorldGameMode.h"
#include "Framework/GamePlatformPlayerStateBase.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformMisc.h"
#include "TimerManager.h"

namespace DivineBeasts::WorldCharacterAdmission
{
bool ParseSelectedCharacter(const FString& Json, const FString& VerifiedPlayerId, FString& OutCharacterId)
{
    OutCharacterId.Reset();
    TSharedPtr<FJsonObject> Object; bool bFound = false; FString PlayerId, CharacterId;
    const TArray<TSharedPtr<FJsonValue>>* Owned = nullptr;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid()
        || !Object->TryGetBoolField(TEXT("found"), bFound) || !bFound
        || !Object->TryGetStringField(TEXT("playerId"), PlayerId) || PlayerId != VerifiedPlayerId
        || !Object->TryGetStringField(TEXT("selectedCharacterId"), CharacterId) || CharacterId.IsEmpty() || CharacterId.Len() > 1024
        || !Object->TryGetArrayField(TEXT("ownedCharacterIds"), Owned)) return false;
    // 当前Profile选择必须属于该已验证玩家的Roster；不以第一个角色或客户端提示填空。
    for (const auto& Value : *Owned)
    {
        FString OwnedId;
        if (Value.IsValid() && Value->TryGetString(OwnedId) && OwnedId == CharacterId)
        { OutCharacterId = CharacterId; return true; }
    }
    return false;
}
bool ParseSelectedHero(const FString& Json, const FString& SelectedCharacterId, FName& OutHeroId)
{
    OutHeroId = NAME_None;
    TSharedPtr<FJsonObject> Object;
    const TArray<TSharedPtr<FJsonValue>>* Roster = nullptr;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid()
        || !Object->TryGetArrayField(TEXT("characters"), Roster) || SelectedCharacterId.IsEmpty()) return false;
    int32 Matches = 0; FName HeroId;
    for (const auto& Value : *Roster)
    {
        const TSharedPtr<FJsonObject>* Entry = nullptr; FString Id, Status, Hero;
        if (!Value.IsValid() || !Value->TryGetObject(Entry) || !Entry->IsValid()
            || !(*Entry)->TryGetStringField(TEXT("characterId"), Id) || Id != SelectedCharacterId) continue;
        if (++Matches != 1 || !(*Entry)->TryGetStringField(TEXT("status"), Status) || Status != TEXT("Active")
            || !(*Entry)->TryGetStringField(TEXT("heroDefinitionId"), Hero) || Hero.Len() > 256
            || !FDivineBeastsHeroCatalog::IsCoreHeroId(FName(*Hero))) return false;
        HeroId = FName(*Hero);
    }
    if (Matches != 1) return false;
    OutHeroId = HeroId; return true;
}
}

FDivineBeastsWorldCharacterAdmission::FDivineBeastsWorldCharacterAdmission(
    ADivineBeastsWorldGameMode& InMode, UGamePlatformServerAdmissionSubsystem& InAdmission)
    : Mode(&InMode), Admission(&InAdmission)
{
    // 专用服务器内网地址显式部署，不用公网网关Token或客户端配置，也不默认猜测生产服务地址。
    PlayerDataBaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("PLAYERDATA_BASE_URL"));
    PlayerDataBaseUrl.RemoveFromEnd(TEXT("/"));
    if (!(PlayerDataBaseUrl.StartsWith(TEXT("http://")) || PlayerDataBaseUrl.StartsWith(TEXT("https://")))
        || PlayerDataBaseUrl.Contains(TEXT("\r")) || PlayerDataBaseUrl.Contains(TEXT("\n"))) PlayerDataBaseUrl.Reset();
}
FDivineBeastsWorldCharacterAdmission::~FDivineBeastsWorldCharacterAdmission() { Close(); }
void FDivineBeastsWorldCharacterAdmission::ObserveAdmissions()
{
    if (bClosed || !Mode.IsValid() || !Admission.IsValid() || AdmissionChangedHandle.IsValid()) return;
    const TWeakPtr<FDivineBeastsWorldCharacterAdmission> WeakThis = AsShared();
    AdmissionChangedHandle = Admission->OnAdmissionChanged().AddWeakLambda(Mode.Get(),
        [WeakThis](const APlayerController* C, const FGamePlatformServerVerifiedAdmission& A, bool bAccepted)
        { if (auto Self = WeakThis.Pin()) Self->HandleAdmission(C, A, bAccepted); });
}
void FDivineBeastsWorldCharacterAdmission::ReleaseConnection(const TSharedPtr<FConnection>& Connection)
{
    Connection->OperationId.Invalidate();
    if (Connection->Request) { Connection->Request->OnProcessRequestComplete().Unbind(); Connection->Request->CancelRequest(); Connection->Request.Reset(); }
    if (Connection->PlayerState.IsValid()) Connection->PlayerState->OnLifecycleChanged.Remove(Connection->LifecycleHandle);
    Connection->LifecycleHandle.Reset();
}
void FDivineBeastsWorldCharacterAdmission::Close()
{
    if (bClosed) return;
    bClosed = true;
    if (Admission.IsValid()) Admission->OnAdmissionChanged().Remove(AdmissionChangedHandle);
    AdmissionChangedHandle.Reset();
    for (const auto& Pair : Connections) ReleaseConnection(Pair.Value);
    Connections.Reset(); Mode.Reset(); Admission.Reset();
}
bool FDivineBeastsWorldCharacterAdmission::IsCurrent(APlayerController* Controller, const TSharedPtr<FConnection>& Connection) const
{
    if (bClosed || !Mode.IsValid() || !Admission.IsValid() || !IsValid(Controller) || !Connection->OperationId.IsValid()
        || Controller->GetWorld() != Mode->GetWorld() || !Mode->GetWorld() || Mode->GetWorld()->bIsTearingDown
        || Connections.FindRef(Controller) != Connection) return false;
    FGamePlatformServerVerifiedAdmission Current;
    return Admission->GetVerifiedAdmission(*Controller, Current) && Current.IsStructurallyValid()
        && Current.AdmissionId == Connection->Verified.AdmissionId && Current.ConnectionId == Connection->Verified.ConnectionId
        && Current.ConnectionGeneration == Connection->Verified.ConnectionGeneration && Current.SessionEpoch == Connection->Verified.SessionEpoch
        && Current.PlayerId == Connection->Verified.PlayerId;
}
void FDivineBeastsWorldCharacterAdmission::HandleAdmission(const APlayerController* Controller,
    const FGamePlatformServerVerifiedAdmission& Verified, bool bAccepted)
{
    check(IsInGameThread());
    if (bClosed || !Mode.IsValid() || !Controller || Controller->GetWorld() != Mode->GetWorld()) return;
    auto* Mutable = const_cast<APlayerController*>(Controller);
    auto Existing = Connections.FindRef(Mutable);
    // 旧准入撤销不能清掉后继连接；同一已接纳事件不重复发请求或初始化GAS。
    if (Existing && Existing->Verified.AdmissionId != Verified.AdmissionId && !bAccepted) return;
    if (Existing && bAccepted && IsCurrent(Mutable, Existing) && Existing->Verified.AdmissionId == Verified.AdmissionId) return;
    if (Existing) { Connections.Remove(Mutable); ReleaseConnection(Existing); }
    if (!bAccepted) { Mode->RevokeVerifiedAdmission(*Mutable, {Verified.AdmissionId, Verified.ConnectionGeneration, Verified.SessionEpoch}); return; }
    auto Connection = MakeShared<FConnection>(); Connection->Verified = Verified; Connection->OperationId = FGuid::NewGuid();
    Connections.Add(Mutable, Connection);
    ReadProfile(*Mutable, Connection);
}
void FDivineBeastsWorldCharacterAdmission::RequestJson(APlayerController& Controller, const TSharedPtr<FConnection>& Connection,
    const FString& Route, TFunction<void(FString)> Completed)
{
    if (!IsCurrent(&Controller, Connection)) return;
    if (PlayerDataBaseUrl.IsEmpty()) { Fail(Controller, Connection, TEXT("WorldCharacterPlayerDataUnconfigured")); return; }
    const TWeakPtr<FDivineBeastsWorldCharacterAdmission> WeakThis = AsShared();
    const TWeakObjectPtr<APlayerController> WeakController = &Controller;
    auto Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(PlayerDataBaseUrl + Route + TEXT("?playerId=") + FGenericPlatformHttp::UrlEncode(Connection->Verified.PlayerId));
    Request->SetVerb(TEXT("GET")); Request->SetTimeout(5.0f);
    Request->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
    Request->OnProcessRequestComplete().BindLambda([WeakThis, WeakController, Connection, Completed = MoveTemp(Completed)](
        FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
    {
        auto Self = WeakThis.Pin(); auto* C = WeakController.Get();
        if (!Self || !Self->IsCurrent(C, Connection)) return;
        Connection->Request.Reset();
        if (!bSucceeded || !Response || Response->GetResponseCode() != 200 || Response->GetContent().Num() > 2 * 1024 * 1024)
        { Self->Fail(*C, Connection, TEXT("WorldCharacterPlayerDataUnavailable")); return; }
        Completed(Response->GetContentAsString());
    });
    Connection->Request = Request;
    if (!Request->ProcessRequest()) { Connection->Request.Reset(); Fail(Controller, Connection, TEXT("WorldCharacterRequestRejected")); }
}
void FDivineBeastsWorldCharacterAdmission::ReadProfile(APlayerController& Controller, const TSharedPtr<FConnection>& Connection)
{
    const TWeakPtr<FDivineBeastsWorldCharacterAdmission> WeakThis = AsShared(); const TWeakObjectPtr<APlayerController> C = &Controller;
    RequestJson(Controller, Connection, TEXT("/internal/v1/playerdata/profile"), [WeakThis, C, Connection](FString Json)
    {
        auto Self = WeakThis.Pin(); if (!Self || !Self->IsCurrent(C.Get(), Connection)) return;
        if (!DivineBeasts::WorldCharacterAdmission::ParseSelectedCharacter(Json, Connection->Verified.PlayerId, Connection->CharacterId))
        { Self->Fail(*C, Connection, TEXT("WorldCharacterSelectionInvalid")); return; }
        Self->ReadRoster(*C, Connection);
    });
}
void FDivineBeastsWorldCharacterAdmission::ReadRoster(APlayerController& Controller, const TSharedPtr<FConnection>& Connection)
{
    const TWeakPtr<FDivineBeastsWorldCharacterAdmission> WeakThis = AsShared(); const TWeakObjectPtr<APlayerController> C = &Controller;
    RequestJson(Controller, Connection, TEXT("/internal/v1/playerdata/characters"), [WeakThis, C, Connection](FString Json)
    {
        auto Self = WeakThis.Pin(); if (!Self || !Self->IsCurrent(C.Get(), Connection)) return;
        if (!DivineBeasts::WorldCharacterAdmission::ParseSelectedHero(Json, Connection->CharacterId, Connection->HeroId))
        { Self->Fail(*C, Connection, TEXT("WorldCharacterRosterInvalid")); return; }
        Self->PublishAdmission(*C, Connection);
    });
}
void FDivineBeastsWorldCharacterAdmission::PublishAdmission(APlayerController& Controller, const TSharedPtr<FConnection>& Connection)
{
    auto* State = Controller.GetPlayerState<AGamePlatformPlayerStateBase>();
    if (!IsCurrent(&Controller, Connection)) return;
    if (!State) { Fail(Controller, Connection, TEXT("WorldCharacterPlayerStateMissing")); return; }
    Connection->PlayerState = State;
    const TWeakPtr<FDivineBeastsWorldCharacterAdmission> WeakThis = AsShared(); const TWeakObjectPtr<APlayerController> C = &Controller;
    // 先订阅再Submit：平台可以同步推进到Possessed；不能只在HTTP完成瞬间检查一次尚不存在的Pawn。
    Connection->LifecycleHandle = State->OnLifecycleChanged.AddWeakLambda(State, [WeakThis, C, Connection]()
    { if (auto Self = WeakThis.Pin(); Self && Self->IsCurrent(C.Get(), Connection)) Self->BindSpawnedPawn(*C, Connection); });
    const auto& A = Connection->Verified; FGamePlatformVerifiedPlayerContext V;
    V.AdmissionId=A.AdmissionId; V.AssignmentId=A.AssignmentId; V.ServerInstanceId=A.ServerInstanceId;
    V.ServerStartGeneration=1; V.ConnectionGeneration=A.ConnectionGeneration; V.SessionEpoch=A.SessionEpoch;
    FGamePlatformId::TryCreate(TEXT("divinebeasts.participant"), TEXT("player_")+A.AdmissionId.ToString(EGuidFormats::Digits), 1, V.ParticipantId);
    FGamePlatformId::TryParse(A.ExperienceId+TEXT("@1"), V.ExperienceId);
    const auto Result = Mode->SubmitVerifiedAdmission(Controller, V);
    if (IsCurrent(&Controller, Connection) && !Result.IsSuccess()) Fail(Controller, Connection, TEXT("WorldCharacterAdmissionRejected"));
}
void FDivineBeastsWorldCharacterAdmission::BindSpawnedPawn(APlayerController& Controller, const TSharedPtr<FConnection>& Connection)
{
    if (!IsCurrent(&Controller, Connection) || !Connection->PlayerState.IsValid()) return;
    const auto Snapshot = Connection->PlayerState->GetLifecycleSnapshot();
    APawn* Pawn = Snapshot.ControlledPawn;
    if (!Pawn || Pawn != Controller.GetPawn() || Pawn == Connection->BoundPawn.Get() || Snapshot.PawnGeneration <= 0
        || Snapshot.PawnGeneration > MAX_int32 || Snapshot.SpawnGeneration <= 0 || Snapshot.SpawnGeneration > MAX_int32
        || (Snapshot.Stage != EGamePlatformPlayerStage::Possessed && Snapshot.Stage != EGamePlatformPlayerStage::AwaitingClient
            && Snapshot.Stage != EGamePlatformPlayerStage::Active)) return;
    auto* Identity = Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>();
    if (!Identity) { Fail(Controller, Connection, TEXT("WorldCharacterComponentMissing")); return; }
    FGamePlatformCharacterInitializationContext Context;
    Context.CharacterId = Connection->CharacterId; Context.HeroDefinitionId = Connection->HeroId;
    Context.SpawnGeneration = static_cast<int32>(Snapshot.SpawnGeneration); Context.AvatarGeneration = static_cast<int32>(Snapshot.PawnGeneration);
    // 同步Readiness回调可能重入生命周期；先记录本次Pawn，失败走下一时隙撤销而非重复初始化。
    Connection->BoundPawn = Pawn; FString Error;
    if (!Identity->AuthorityBindTrustedContext(Context, Error)) { Fail(Controller, Connection, TEXT("WorldCharacterInitializationRejected")); return; }
    UE_LOG(LogTemp, Display, TEXT("WorldCharacter bound: Pawn=%s Hero=%s Spawn=%d Avatar=%d Position=%s"), *Pawn->GetName(), *Context.HeroDefinitionId.ToString(), Context.SpawnGeneration, Context.AvatarGeneration, *Pawn->GetActorLocation().ToCompactString());
}
void FDivineBeastsWorldCharacterAdmission::Fail(APlayerController& Controller, const TSharedPtr<FConnection>& Connection, FName Code)
{
    if (!IsCurrent(&Controller, Connection)) return;
    UE_LOG(LogTemp, Error, TEXT("WorldCharacter admission failed: %s"), *Code.ToString());
    const TWeakPtr<FDivineBeastsWorldCharacterAdmission> WeakThis = AsShared(); const TWeakObjectPtr<APlayerController> C = &Controller;
    Mode->GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis, C, Connection]()
    {
        auto Self = WeakThis.Pin(); if (!Self || !Self->IsCurrent(C.Get(), Connection)) return;
        const auto A = Connection->Verified;
        // Release发布真实撤销并释放本连接的后端绑定；监听者负责同步清理平台玩家，不伪装仍在线。
        Self->Admission->ReleaseAdmission(*C, {});
        if (Self->Connections.FindRef(C) == Connection) { Self->Connections.Remove(C); Self->ReleaseConnection(Connection); }
    }));
}

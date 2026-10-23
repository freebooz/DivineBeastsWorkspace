// 神兽联盟客户端Village地形草地/雪层装配；历史Snow类型及配置身份保持兼容。
// 仅在经检验的新手村客户端World中运行，复用平台WeatherClient值事件和GamePlatformData资源租约。
// DedicatedServer继续仅复制天气事实；客户端的Snow/Normal/Albedo软资源不进入共享地图硬引用。
#include "World/DivineBeastsVillageSnowWorldSubsystem.h"

#include "Settings/DivineBeastsVillageSnowSettings.h"
#include "Subsystems/GamePlatformWeatherClientWorldSubsystem.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Types/GamePlatformWeatherTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "LandscapeComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Interfaces/GamePlatformLandscapeMaterialBridge.h"
#include "Interfaces/IGamePlatformSurfaceService.h"
#include "Components/GamePlatformSnowFootContactComponent.h"
#include "GameFramework/Character.h"
#include "UObject/Package.h"
#if !UE_BUILD_SHIPPING
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#endif


DEFINE_LOG_CATEGORY_STATIC(LogDBAVillageSnow, Log, All);

namespace
{
    /** 服务于Village中性母材质的稳定参数协议，不能放入平台模块反向依赖项目。 */
    const FName SnowAmountName(TEXT("VillageSnowAmount"));
    const FName WetnessName(TEXT("VillageWetness"));
    const FName CoverageName(TEXT("VillageSnowCoverageTexture"));
    const FName AlbedoName(TEXT("VillageSnowAlbedoTexture"));
    const FName NormalName(TEXT("VillageSnowNormalTexture"));
    const FName SnowORMName(TEXT("VillageSnowORMTexture"));
    const FName SnowHeightName(TEXT("VillageSnowHeightTexture"));
    constexpr int32 VillageSurfaceTextureCount = 8;
    const FName GroundAlbedoName(TEXT("VillageGroundAlbedoTexture"));
    const FName GroundNormalName(TEXT("VillageGroundNormalTexture"));
    const FName GroundORMName(TEXT("VillageGroundORMTexture"));
    const FString ExpectedBridgePath(TEXT(
        "/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge"));
}

bool UDivineBeastsVillageSnowWorldSubsystem::IsVillageWorldPackage(const FString& PackageName)
{
    // PIE副本可能以UEDPIE_N_前缀重写资产短名，但仍在真实WorldPack挂载路径下。
    return PackageName.Contains(TEXT("/DBAWorldPack_Village/Maps/")) &&
        PackageName.EndsWith(TEXT("L_Village_Start"), ESearchCase::CaseSensitive);
}

bool UDivineBeastsVillageSnowWorldSubsystem::MakeClampedVisualState(
    const FGamePlatformWeatherState& State, float& OutSnowAmount, float& OutWetness)
{
    if (!State.IsValid()) return false;
    OutSnowAmount = FMath::Clamp(State.SnowAmount, 0.f, 1.f);
    OutWetness = FMath::Clamp(State.Wetness, 0.f, 1.f);
    return true;
}

bool UDivineBeastsVillageSnowWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return IsValid(World) &&
        (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) &&
        World->GetNetMode() != NM_DedicatedServer && !IsRunningCommandlet();
}

void UDivineBeastsVillageSnowWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bClosing = false;
    bVillageWorld = false;
    bTexturesReady = false;
    TexturesAppliedLandscapes.Reset();
    bLoggedMissingMaterial = false;
    RequestGeneration = 0;
    MossRequestGeneration = 0;
    bMossMaterialReady = false;
    LoadedMossMaterial.Reset();
    MossMaterialSlotBindings.Reset();
    RetainedMossOriginalMaterials.Reset();
#if !UE_BUILD_SHIPPING
    // 仅显式开发参数开启，避免普通天气插值在日志写入端形成性能开销。
    bReviewWeatherTrace = FParse::Param(FCommandLine::Get(), TEXT("DBAVillageSnowTraceWeather"));
    LastTracedSnowAmount = -1.f;
    LastTracedWeatherType = -1;
    bGroundSnowReviewCapture = FParse::Param(FCommandLine::Get(), TEXT("DBAVillageGroundSnowCaptureReview"));
    bReviewScreenshotEnabled = bGroundSnowReviewCapture || FParse::Param(FCommandLine::Get(), TEXT("DBAVillageSnowCaptureReview"));
    bReviewScreenshotQueued = false;
    PendingReviewPhase = NAME_None;
    CapturedReviewPhases.Reset();
#endif
}

void UDivineBeastsVillageSnowWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    const auto* Config = GetDefault<UDivineBeastsVillageSnowSettings>();
    bVillageWorld = Config && Config->bEnableVillageSnowPresentation &&
        IsVillageWorldPackage(InWorld.GetOutermost()->GetName());
    if (!bVillageWorld || InWorld.GetNetMode() == NM_DedicatedServer) return;
    // 苔藓材质的软租约独立于天气桥：即使WeatherClient失败也可保持静态苔藓。
    TryRequestMossMaterial();
    // 草地是村庄基础表面，不应依赖天气通知是否到达；已流送地形也由同一事件处理。
    ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(
        FOnActorSpawned::FDelegate::CreateUObject(
            this, &UDivineBeastsVillageSnowWorldSubsystem::OnLandscapeActorSpawned));
    TryRequestSnowTextures();

    // 首次快照装配一次已存在角色，后续使用同一Actor生成事件，不逐帧扫描。
    for (TActorIterator<ACharacter> It(&InWorld); It; ++It) AttachSnowFootContacts(*It);

    WeatherClient = InWorld.GetSubsystem<UGamePlatformWeatherClientWorldSubsystem>();
    if (!WeatherClient.IsValid())
    {
        UE_LOG(LogDBAVillageSnow, Warning, TEXT("Village客户端未初始化WeatherClient，雪材质保持基础模式。"));
        return;
    }
    WeatherChangedHandle = WeatherClient->AddVisualChangedHandler(
        FGamePlatformWeatherVisualChanged::FDelegate::CreateUObject(
            this, &UDivineBeastsVillageSnowWorldSubsystem::OnVisualWeatherChanged));

    OnVisualWeatherChanged(WeatherClient->GetVisualWeather());
}

void UDivineBeastsVillageSnowWorldSubsystem::OnVisualWeatherChanged(
    const FGamePlatformWeatherState& State)
{
    check(IsInGameThread());
    if (bClosing || !bVillageWorld) return;
    UWorld* World = GetWorld();
    if (!IsValid(World) || World->bIsTearingDown) return;

    float NextSnow = 0.f;
    float NextWetness = 0.f;
    if (!MakeClampedVisualState(State, NextSnow, NextWetness)) return;
    const bool bStateChanged = !FMath::IsNearlyEqual(CurrentSnowAmount, NextSnow, .001f) ||
        !FMath::IsNearlyEqual(CurrentWetness, NextWetness, .001f);
    CurrentSnowAmount = NextSnow;
    CurrentWetness = NextWetness;

    // 首帧必须为已有Landscape写入环境快照；后续仅在真实值变化时更新。
    if (bStateChanged || !bTexturesReady)
    {
        for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
        {
            ApplyToLandscape(*It);
        }
    }
    if (!bTexturesReady && !TextureLease.IsValid())
    {
        TryRequestSnowTextures();
    }
#if !UE_BUILD_SHIPPING
    TraceWeatherVisualForReview(State);
#endif
}

void UDivineBeastsVillageSnowWorldSubsystem::TryRequestSnowTextures()
{
    check(IsInGameThread());
    UWorld* World = GetWorld();
    const auto* Config = GetDefault<UDivineBeastsVillageSnowSettings>();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    IGamePlatformDataService* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    if (!Data || bClosing || !Config || TextureLease.IsValid() || !bVillageWorld) return;

    const TArray<FSoftObjectPath> Paths = {
        Config->SnowCoverageTexture, Config->SnowAlbedoTexture, Config->SnowNormalTexture,
        Config->SnowORMTexture, Config->SnowHeightTexture,
        Config->GroundAlbedoTexture, Config->GroundNormalTexture, Config->GroundORMTexture
    };
    TSet<FSoftObjectPath> UniquePaths;
    for (const FSoftObjectPath& Path : Paths) if (Path.IsValid()) UniquePaths.Add(Path);
    if (UniquePaths.Num() != VillageSurfaceTextureCount)
    {
        UE_LOG(LogDBAVillageSnow, Error, TEXT("Village客户端八张草地/雪纹理配置为空、非法或重复，拒绝异步加载。"));
        return;
    }

    const int64 Generation = ++RequestGeneration;
    const TWeakObjectPtr<UDivineBeastsVillageSnowWorldSubsystem> WeakThis(this);
    FGamePlatformResult Accepted;
    TextureLease = Data->AcquireResources(Paths, EGamePlatformDataLifetime::World, this,
        [WeakThis, Generation](const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
        {
            if (auto* Self = WeakThis.Get())
                Self->HandleTexturesReady(Generation, Lease, Result);
        }, Accepted);
    if (!Accepted.IsSuccess() || !TextureLease.IsValid())
    {
        UE_LOG(LogDBAVillageSnow, Warning,
            TEXT("Village雪纹理资源租约未被接纳；保留世界基础材质等待新天气事件。"));
    }
}

void UDivineBeastsVillageSnowWorldSubsystem::HandleTexturesReady(
    int64 ExpectedGeneration, const FGamePlatformDataLease& CompletedLease,
    const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    UWorld* World = GetWorld();
    if (bClosing || !bVillageWorld || !IsValid(World) || World->bIsTearingDown ||
        ExpectedGeneration != RequestGeneration || !TextureLease.IsValid() ||
        CompletedLease.LeaseId != TextureLease.LeaseId ||
        CompletedLease.ScopeId != TextureLease.ScopeId ||
        CompletedLease.Generation != TextureLease.Generation ||
        CompletedLease.IssuerProof != TextureLease.IssuerProof ||
        CompletedLease.ResourcePaths != TextureLease.ResourcePaths) return;

    UGameInstance* GameInstance = World->GetGameInstance();
    IGamePlatformDataService* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    bool bAllTexturesLoaded = CompletedLease.ResourcePaths.Num() == VillageSurfaceTextureCount;
    for (const FSoftObjectPath& Path : CompletedLease.ResourcePaths)
        bAllTexturesLoaded = bAllTexturesLoaded && IsValid(Cast<UTexture2D>(Path.ResolveObject()));
    if (!Data || !Result.IsSuccess() ||
        Data->GetLeaseState(CompletedLease) != EGamePlatformDataRequestState::Succeeded ||
        !bAllTexturesLoaded)
    {
        UE_LOG(LogDBAVillageSnow, Warning,
            TEXT("Village草地/雪Texture2D异步租约失败或类别错误，维持共享地形材质默认外观。"));
        if (Data) Data->ReleaseResources(CompletedLease);
        TextureLease = {};
        ++RequestGeneration; // 只允许下一次有变化的天气事件重新发起，杜绝同步重试循环。
        return;
    }

    bTexturesReady = true;
    // 新的资源租约完成时允许首次写入；正常天气过渡不得每0.1秒重绑高精贴图。
    TexturesAppliedLandscapes.Reset();
    int32 BoundLandscapeCount = 0;
    for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
    {
        if (ApplyToLandscape(*It)) ++BoundLandscapeCount;
    }
    if (BoundLandscapeCount > 0)
    {
        UE_LOG(LogDBAVillageSnow, Log,
            TEXT("VILLAGE_GROUND_MID_VERIFY_SUCCESS VILLAGE_SNOW_MID_VERIFY_SUCCESS：%d处Landscape的草地/雪八纹理、雪量及湿润已写入并回读一致。"),
            BoundLandscapeCount);
    }
    else
    {
        UE_LOG(LogDBAVillageSnow, Error,
            TEXT("VILLAGE_SNOW_MID_VERIFY_FAILED：雪纹理虽加载，但未能证实实际Landscape动态材质成功绑定。"));
    }
#if !UE_BUILD_SHIPPING
    if (BoundLandscapeCount > 0 && WeatherClient.IsValid())
    {
        TraceWeatherVisualForReview(WeatherClient->GetVisualWeather());
    }
#endif
}

void UDivineBeastsVillageSnowWorldSubsystem::OnLandscapeActorSpawned(AActor* Actor)
{
    if (!bClosing && bVillageWorld)
    {
        if (ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(Actor))
        {
            ApplyToLandscape(Proxy);
        }
        // 只处理标记过的项目岩石组件；其余Actor绝不覆盖材质。
        ApplyMossMaterialToActor(Actor);
        AttachSnowFootContacts(Actor);
    }
}

void UDivineBeastsVillageSnowWorldSubsystem::AttachSnowFootContacts(AActor* Actor)
{
    auto* Character = Cast<ACharacter>(Actor);
    if (bClosing || !bVillageWorld || !IsValid(Character) || Character->GetWorld()!=GetWorld() ||
        Character->FindComponentByClass<UGamePlatformSnowFootContactComponent>()) return;
    auto* Component=NewObject<UGamePlatformSnowFootContactComponent>(Character);
    Component->SetContactMesh(Character->GetMesh());
    Character->AddInstanceComponent(Component);
    Component->RegisterComponent();
    SnowFootContactComponents.Add(Component);
}

bool UDivineBeastsVillageSnowWorldSubsystem::ApplyToLandscape(ALandscapeProxy* Landscape)
{
    if (!IsValid(Landscape) || bClosing || !bVillageWorld ||
        Landscape->GetWorld() != GetWorld() || !Landscape->bUseDynamicMaterialInstance) return false;

    UMaterialInterface* Material = Landscape->LandscapeMaterial;
    UMaterialInterface* EffectiveMaterial = Landscape->GetLandscapeMaterial();
    // 如果运行时被Landscape的流送代理或动态实例包装，记录实际原始材质与最终求值材质；
    // 在未确认跨版本引用变化之前不强行修改共享地图或在运行时替换材质。
    if (!IsValid(Material) || !Material->GetPathName().StartsWith(ExpectedBridgePath))
    {
        if (!bLoggedMissingMaterial)
        {
            UE_LOG(LogDBAVillageSnow, Warning,
                TEXT("Village中性雪层材质不匹配：Landscape=%s 原始=%s 求值=%s 期望=%s。"),
                *GetPathNameSafe(Landscape), *GetPathNameSafe(Material),
                *GetPathNameSafe(EffectiveMaterial), *ExpectedBridgePath);
            bLoggedMissingMaterial = true;
        }
        return false;
    }

    bool bHasDynamicInstance = false;
    for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
    {
        if (!Component) continue;
        for (UMaterialInstanceDynamic* DynamicMaterial : Component->MaterialInstancesDynamic)
        {
            if (IsValid(DynamicMaterial))
            {
                bHasDynamicInstance = true;
                break;
            }
        }
        if (bHasDynamicInstance) break;
    }
    if (!bHasDynamicInstance)
    {
        if (!bLoggedMissingMaterial)
        {
            UE_LOG(LogDBAVillageSnow, Warning,
                TEXT("Village Landscape未烘焙动态材质实例，必须在地图资产中启用bUseDynamicMaterialInstance。"));
            bLoggedMissingMaterial = true;
        }
        return false;
    }

    Landscape->SetLandscapeMaterialScalarParameterValue(SnowAmountName, CurrentSnowAmount);
    Landscape->SetLandscapeMaterialScalarParameterValue(WetnessName, CurrentWetness);
    if (!bTexturesReady) return true; // 仅标量基础模式，不记入纹理验证成功集合。
    if (TextureLease.ResourcePaths.Num() != VillageSurfaceTextureCount) return false;
    // 雪量插值频繁变化，只需更新标量；已回读成功的贴图应由当前世界长期租约持有。
    if (TexturesAppliedLandscapes.Contains(TWeakObjectPtr<ALandscapeProxy>(Landscape))) return true;

    const auto* Config = GetDefault<UDivineBeastsVillageSnowSettings>();
    if (!Config) return false;
    // Data对资源路径排序；按语义配置路径查找且要求当前租约持有，不能按输入顺序/数组下标猜用途。
    const auto ResolveOwnedTexture = [this](const FSoftObjectPath& Path) -> UTexture2D*
    {
        return TextureLease.ResourcePaths.Contains(Path) ? Cast<UTexture2D>(Path.ResolveObject()) : nullptr;
    };
    UTexture2D* Coverage = ResolveOwnedTexture(Config->SnowCoverageTexture);
    UTexture2D* Albedo = ResolveOwnedTexture(Config->SnowAlbedoTexture);
    UTexture2D* Normal = ResolveOwnedTexture(Config->SnowNormalTexture);
    UTexture2D* SnowORM = ResolveOwnedTexture(Config->SnowORMTexture);
    UTexture2D* SnowHeight = ResolveOwnedTexture(Config->SnowHeightTexture);
    UTexture2D* GroundAlbedo = ResolveOwnedTexture(Config->GroundAlbedoTexture);
    UTexture2D* GroundNormal = ResolveOwnedTexture(Config->GroundNormalTexture);
    UTexture2D* GroundORM = ResolveOwnedTexture(Config->GroundORMTexture);
    if (!IsValid(Coverage) || !IsValid(Albedo) || !IsValid(Normal) ||
        !IsValid(SnowORM) || !IsValid(SnowHeight) ||
        !IsValid(GroundAlbedo) || !IsValid(GroundNormal) || !IsValid(GroundORM)) return false;
    Landscape->SetLandscapeMaterialTextureParameterValue(CoverageName, Coverage);
    Landscape->SetLandscapeMaterialTextureParameterValue(AlbedoName, Albedo);
    Landscape->SetLandscapeMaterialTextureParameterValue(NormalName, Normal);
    Landscape->SetLandscapeMaterialTextureParameterValue(SnowORMName, SnowORM);
    Landscape->SetLandscapeMaterialTextureParameterValue(SnowHeightName, SnowHeight);
    Landscape->SetLandscapeMaterialTextureParameterValue(GroundAlbedoName, GroundAlbedo);
    Landscape->SetLandscapeMaterialTextureParameterValue(GroundNormalName, GroundNormal);
    Landscape->SetLandscapeMaterialTextureParameterValue(GroundORMName, GroundORM);
    // 旧纯色地面Tint不能再次将扫描草地变成深灰褐色；晴天保留源色，雨湿仅自然降低反射底色。
    Landscape->SetLandscapeMaterialVectorParameterValue(TEXT("VillageDryColor"), FLinearColor::White);
    Landscape->SetLandscapeMaterialVectorParameterValue(TEXT("VillageWetColor"), FLinearColor(.55f, .60f, .52f, 1.f));

    // 不能以Setter调用或Data租约返回成功代替真正的运行时表现绑定。
    // 每个已有MID回读八个Texture、两个天气数值及干燥白色乘色；少一个都视为未通过。
    int32 MidCount = 0;
    int32 VerifiedMidCount = 0;
    for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
    {
        if (!IsValid(Component)) continue;
        for (UMaterialInstanceDynamic* Mid : Component->MaterialInstancesDynamic)
        {
            if (!IsValid(Mid)) continue;
            ++MidCount;
            if (Mid->K2_GetTextureParameterValue(CoverageName) == Coverage &&
                Mid->K2_GetTextureParameterValue(AlbedoName) == Albedo &&
                Mid->K2_GetTextureParameterValue(NormalName) == Normal &&
                Mid->K2_GetTextureParameterValue(SnowORMName) == SnowORM &&
                Mid->K2_GetTextureParameterValue(SnowHeightName) == SnowHeight &&
                Mid->K2_GetTextureParameterValue(GroundAlbedoName) == GroundAlbedo &&
                Mid->K2_GetTextureParameterValue(GroundNormalName) == GroundNormal &&
                Mid->K2_GetTextureParameterValue(GroundORMName) == GroundORM &&
                Mid->K2_GetVectorParameterValue(TEXT("VillageDryColor")).Equals(FLinearColor::White, .002f) &&
                FMath::IsNearlyEqual(Mid->K2_GetScalarParameterValue(SnowAmountName), CurrentSnowAmount, .002f) &&
                FMath::IsNearlyEqual(Mid->K2_GetScalarParameterValue(WetnessName), CurrentWetness, .002f))
            {
                ++VerifiedMidCount;
            }
        }
    }
    if (MidCount == 0 || VerifiedMidCount != MidCount)
    {
        UE_LOG(LogDBAVillageSnow, Warning,
            TEXT("Village Landscape MID贴图/天气参数回读不完整：%d/%d，地形=%s"),
            VerifiedMidCount, MidCount, *GetPathNameSafe(Landscape));
        return false;
    }
    int32 NaniteSlots = 0;
    FString NaniteDiagnostic;
    if (!BindGamePlatformLandscapeNaniteMIDs(*Landscape, NaniteSlots, NaniteDiagnostic))
    {
        UE_LOG(LogDBAVillageSnow, Warning, TEXT("Village Nanite雪材质绑定未就绪：%s"), *NaniteDiagnostic);
        return false;
    }
    if (NaniteSlots > 0)
    {
        // 日志也从已就绪的真实MID读取；地形组件数组可能保留空槽，不能假定首槽有效。
        float VerifiedDisplacement = 0.f;
        for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
        {
            if (IsValid(Component))
                if (UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Component->GetMaterialInstance(0, true)))
                {
                    VerifiedDisplacement = Mid->K2_GetScalarParameterValue(TEXT("Displacement Multiplier"));
                    break;
                }
        }
        UE_LOG(LogDBAVillageSnow, Display, TEXT("VILLAGE_SNOW_NANITE_MID_VERIFY_SUCCESS slots=%d displacement=%.3f"),
            NaniteSlots, VerifiedDisplacement);
    }
    if (auto* Surface=IGamePlatformSurfaceService::Get(*GetWorld()))
        for (const auto& Component:Landscape->LandscapeComponents)
            if (IsValid(Component))
                for (const auto& Mid:Component->MaterialInstancesDynamic)
                    if (IsValid(Mid)) Surface->RegisterSnowContactMaterial(*Mid);
    TexturesAppliedLandscapes.Add(Landscape);
    return true;
}


#if !UE_BUILD_SHIPPING
void UDivineBeastsVillageSnowWorldSubsystem::TraceWeatherVisualForReview(
    const FGamePlatformWeatherState& State)
{
    if (!bReviewWeatherTrace || !bTexturesReady || bClosing || !bVillageWorld) return;
    const int32 Type = static_cast<int32>(State.Type);
    FName ReviewPhase;
    if (bGroundSnowReviewCapture)
    {
        if (State.Type == EGamePlatformWeatherType::LightSnow && CurrentSnowAmount >= .225f && CurrentSnowAmount <= .235f)
            ReviewPhase = TEXT("LightSnow");
        else if (State.Type == EGamePlatformWeatherType::HeavySnow && CurrentSnowAmount >= .845f)
            ReviewPhase = CapturedReviewPhases.Contains(FName(TEXT("MeltedGround")))
                ? FName(TEXT("HeldHeavySnow")) : FName(TEXT("HeavySnow"));
        else if (State.Type == EGamePlatformWeatherType::Cloudy && State.SnowIntensity <= .002f)
        {
            if (CurrentSnowAmount >= .845f && State.TemperatureCelsius < 0.f) ReviewPhase = TEXT("StoppedSnow");
            else if (CurrentSnowAmount >= .39f && CurrentSnowAmount <= .44f) ReviewPhase = TEXT("MeltingSnow");
            else if (CurrentSnowAmount <= .002f && CurrentWetness >= .64f) ReviewPhase = TEXT("MeltedGround");
        }
    }
    const bool bNewCapturePhase = !ReviewPhase.IsNone() && !CapturedReviewPhases.Contains(ReviewPhase);
    if (!bNewCapturePhase && LastTracedSnowAmount >= 0.f &&
        FMath::Abs(CurrentSnowAmount - LastTracedSnowAmount) < .085f &&
        LastTracedWeatherType == Type)
    {
        return; // 只记录有观察意义的变化，禁止每0.1秒把全部采样写入日志。
    }
    const UWorld* World = GetWorld();
    if (!IsValid(World) || World->bIsTearingDown) return;
    int32 ActiveMids = 0;
    int32 MatchingMids = 0;
    float SampleSnow = -1.f;
    for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
    {
        const ALandscapeProxy* Landscape = *It;
        if (!IsValid(Landscape) ||
            !TexturesAppliedLandscapes.Contains(TWeakObjectPtr<ALandscapeProxy>(*It)))
        {
            continue;
        }
        for (const ULandscapeComponent* Component : Landscape->LandscapeComponents)
        {
            if (!IsValid(Component)) continue;
            for (UMaterialInstanceDynamic* Mid : Component->MaterialInstancesDynamic)
            {
                if (!IsValid(Mid)) continue;
                ++ActiveMids;
                SampleSnow = Mid->K2_GetScalarParameterValue(SnowAmountName);
                if (FMath::IsNearlyEqual(SampleSnow, CurrentSnowAmount, .005f) &&
                    FMath::IsNearlyEqual(Mid->K2_GetScalarParameterValue(WetnessName),
                                         CurrentWetness, .005f))
                {
                    ++MatchingMids;
                }
            }
        }
    }
    const bool bMatched = ActiveMids > 0 && ActiveMids == MatchingMids;
    UE_LOG(LogDBAVillageSnow, Display,
        TEXT("VILLAGE_SNOW_NET_VISUAL_SAMPLE type=%d snow=%.3f wet=%.3f intensity=%.3f mid=%.3f matching=%d count=%d"),
        Type, CurrentSnowAmount, CurrentWetness,
        State.SnowIntensity, SampleSnow, bMatched ? 1 : 0, ActiveMids);
    // 即便MID尚未可见也记录失败状态，绝不能把天气值接收当作雪层绑定成功。
    LastTracedSnowAmount = CurrentSnowAmount;
    LastTracedWeatherType = Type;
    // 仅在数值和MID均验证通过时安排原生截图。历史参数仍只截大雪一次，
    // 地面审核参数分别截薄雪、实覆、停雪、融化中途和融化完成；正式客户端默认关闭。
    if (bMatched && bReviewScreenshotEnabled && !bReviewScreenshotQueued &&
        ((bGroundSnowReviewCapture && bNewCapturePhase) ||
         (!bGroundSnowReviewCapture && CurrentSnowAmount >= .78f && State.Type == EGamePlatformWeatherType::HeavySnow)))
    {
        if (UWorld* MutableWorld = GetWorld())
        {
            bReviewScreenshotQueued = true;
            PendingReviewPhase = bGroundSnowReviewCapture ? ReviewPhase : FName(TEXT("HeavySnow"));
            CapturedReviewPhases.Add(PendingReviewPhase);
            MutableWorld->GetTimerManager().SetTimer(
                ReviewScreenshotTimer, this,
                &UDivineBeastsVillageSnowWorldSubsystem::CaptureWeatherReviewScreenshot,
                2.0f, false);
        }
    }
}

void UDivineBeastsVillageSnowWorldSubsystem::CaptureWeatherReviewScreenshot()
{
    UWorld* World = GetWorld();
    UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
    UGameViewportClient* Viewport = Instance ? Instance->GetGameViewportClient() : nullptr;
    if (bClosing || !bVillageWorld || !IsValid(World) || World->bIsTearingDown ||
        (!bGroundSnowReviewCapture && CurrentSnowAmount < .75f) || !IsValid(Viewport) || !Viewport->Viewport)
    {
        UE_LOG(LogDBAVillageSnow, Warning,
            TEXT("VILLAGE_SNOW_REVIEW_SCREENSHOT_SKIPPED：天气审核条件或真实游戏视口不可用。"));
        return;
    }
    FString Filename = FString::Printf(
        TEXT("VillageSnow_%s_PID%u_%s.png"), *PendingReviewPhase.ToString(), FPlatformProcess::GetCurrentProcessId(),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")));
    FString ReviewDirectory;
    if (bGroundSnowReviewCapture && FParse::Value(FCommandLine::Get(), TEXT("DBAVillageGroundSnowMediaRoot="), ReviewDirectory) &&
        !ReviewDirectory.IsEmpty() && !FPaths::IsRelative(ReviewDirectory))
    {
        // 仅显式开发参数指定输出目录；唯一PID避免两个客户端同秒截图互相覆盖。
        Filename = FPaths::Combine(ReviewDirectory, Filename);
    }
    // false排除UMG/Slate悬浮层，保留原生场景像素。双客户端身份另由进程和日志绑定。
    FScreenshotRequest::RequestScreenshot(Filename, false, false);
    bReviewScreenshotQueued = !bGroundSnowReviewCapture;
    UE_LOG(LogDBAVillageSnow, Display,
        TEXT("VILLAGE_SNOW_REVIEW_SCREENSHOT_REQUESTED：真实GPU帧已请求，文件名=%s（仍需核实是否实际落盘及像素内容）。"),
        *Filename);
}
#endif

void UDivineBeastsVillageSnowWorldSubsystem::ReleaseResources()
{
    ++RequestGeneration;
    bTexturesReady = false;
    // 释放异步材质资源前撤销本次世界的显式网格覆盖，避免资源卸载留下悬空MID。
    ++MossRequestGeneration;
    for (const FMossMaterialSlotBinding& Binding : MossMaterialSlotBindings)
    {
        UMeshComponent* Component = Binding.Component.Get();
        if (!IsValid(Component) || !RetainedMossOriginalMaterials.IsValidIndex(Binding.RetainedMaterialIndex) ||
            Binding.MaterialSlot < 0 || Binding.MaterialSlot >= Component->GetNumMaterials())
        {
            continue;
        }
        if (Component->GetMaterial(Binding.MaterialSlot) == LoadedMossMaterial.Get())
        {
            Component->SetMaterial(Binding.MaterialSlot,
                RetainedMossOriginalMaterials[Binding.RetainedMaterialIndex].Get());
        }
    }
    MossMaterialSlotBindings.Reset();
    RetainedMossOriginalMaterials.Reset();
    LoadedMossMaterial.Reset();
    bMossMaterialReady = false;
    TexturesAppliedLandscapes.Reset();
#if !UE_BUILD_SHIPPING
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ReviewScreenshotTimer);
    }
    bReviewScreenshotQueued = false;
#endif
    const FGamePlatformDataLease OldLease = TextureLease;
    TextureLease = {};
    const FGamePlatformDataLease OldMossLease = MossMaterialLease;
    MossMaterialLease = {};
    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    if (OldLease.IsValid() && GameInstance)
    {
        if (IGamePlatformDataService* Data = IGamePlatformDataService::Get(*GameInstance))
            Data->ReleaseResources(OldLease);
    }
    if (OldMossLease.IsValid() && GameInstance)
    {
        if (IGamePlatformDataService* Data = IGamePlatformDataService::Get(*GameInstance))
            Data->ReleaseResources(OldMossLease);
    }
}

void UDivineBeastsVillageSnowWorldSubsystem::Deinitialize()
{
    bClosing = true;
    if (WeatherClient.IsValid() && WeatherChangedHandle.IsValid())
        WeatherClient->RemoveVisualChangedHandler(WeatherChangedHandle);
    WeatherChangedHandle.Reset();
    WeatherClient.Reset();
    if (UWorld* World = GetWorld(); World && ActorSpawnedHandle.IsValid())
        World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
    ActorSpawnedHandle.Reset();
    for (const auto& Component:SnowFootContactComponents) if (Component.IsValid()) Component->DestroyComponent();
    SnowFootContactComponents.Reset();
    ReleaseResources();
    bVillageWorld = false;
    Super::Deinitialize();
}

/**
 * 项目层本地玩家角色预览：接收界面ViewModel命令，组合平台预览舞台和项目外观软引用。
 * 仅拥有本地相机、流送工作室及异步资源租约；不改变持久档案、世界角色或服务器动画权威。
 * 切换英雄、失活及世界退出依请求代次取消加载，解绑并释放本实例资源。
 */
#include "Characters/DivineBeastsCharacterPreviewSubsystem.h"

#include "Animation/AnimInstance.h" // 本文件也调用动画软类Get，完整类型不能由另一个Unity源文件提供。
#include "Characters/DivineBeastsCharacterAppearanceCatalog.h"
#include "Cinematic/GamePlatformCinematicCameraProfile.h"
#include "Cinematic/GamePlatformCinematicCameraSubsystem.h"
#include "Camera/CameraComponent.h"
#include "DivineBeastsPresentationClientSubsystem.h" // 项目表现层中立请求，不直接加载Niagara。
#include "Characters/DivineBeastsCharacterAppearanceProfile.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h" // 材质读取/设置及组件UObject转换需要完整类型，不能依赖PCH或Unity包含顺序。
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/Level.h" // 查询当前流送工作室的地面Actor归属。
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h" // 本文件读取Profile软网格引用，资产类型也必须直接完整包含。
#include "Engine/World.h"
#include "TimerManager.h"
#include "CollisionQueryParams.h" // 仅本地可见地形采样，绝不用于权威Gameplay碰撞与网络同步。
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DefaultPawn.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Preview/GamePlatformCharacterPreviewStage.h"

// 只记录项目资源身份和生命周期代次，便于定位预览失败；不记录用户账号、密码或票据。
DEFINE_LOG_CATEGORY_STATIC(LogDivineBeastsCharacterPreview, Log, All);

namespace
{
    // 开发原型的待机只归本地预览；不写入通用角色外观Profile，不覆盖游戏内移动／权威动画。
    const TSoftClassPtr<UAnimInstance> DevelopmentPreviewIdleClass(
        FSoftObjectPath(TEXT("/DBAContentPack_Common/Mannequins/DBA/Animations/ABP_DBA_PreviewIdle.ABP_DBA_PreviewIdle_C")));
    const TSoftObjectPtr<UWorld> CharacterStudioWorld(
        FSoftObjectPath(
            TEXT("/DBAFrontEndPack/Maps/L_DBA_CharacterStudio.L_DBA_CharacterStudio")));
}


EDivineBeastsCharacterStudioWaterMode
UDivineBeastsCharacterPreviewSubsystem::GetCharacterStudioWaterMode(
    bool bPreferFluidFlux, bool bHighQuality) const
{
    return UDivineBeastsCharacterStudioWaterPolicy::ResolveWaterMode(
        PreviewStage.Get(), bPreferFluidFlux, bHighQuality);
}

void UDivineBeastsCharacterPreviewSubsystem::SetPreviewFoliageStyle(FName StyleId)
{
    // 蓝图可按项目内容选择外观；只允许现有已登记且具有真实Definition的种类。
    if (StyleId != TEXT("Peach") && StyleId != TEXT("Maple") &&
        StyleId != TEXT("Bamboo") && StyleId != TEXT("Ginkgo"))
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Warning,
            TEXT("无效自然飘落外观：%s"), *StyleId.ToString());
        return;
    }
    if (PreviewFoliageStyle == StyleId)
        return;
    PreviewFoliageStyle = StyleId;
    EnsurePreviewFoliage(); // 世界未就绪时由OnLevelShown延迟启用。
}

void UDivineBeastsCharacterPreviewSubsystem::EnsurePreviewFoliage()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
    if (!World || !PreviewStage.IsValid() || World->bIsTearingDown)
        return;
    UDivineBeastsPresentationClientSubsystem* Presentation =
        LocalPlayer->GetSubsystem<UDivineBeastsPresentationClientSubsystem>();
    if (!Presentation)
        return;

    // 预览关卡可能使用Landscape或自定义地表，不能假定存在无碰撞PreviewFloor。
    // 只读取当前角色舞台所属关卡中的锚点；绝不绑定旧流送实例或其他玩家的地面。
    const FVector Origin = PreviewStage->GetActorLocation();
    ULevel* const OwnedLevel = PreviewStage->GetLevel();
    if (!OwnedLevel)
    {
        return;
    }

    float GroundZ = Origin.Z - 1.f; // 缺锚点时只为早期工作室保留明确的平面退化表现。
    double NearestDistanceSquared = TNumericLimits<double>::Max();
    bool bFoundGroundAnchor = false;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetLevel() != OwnedLevel ||
            !It->ActorHasTag(TEXT("DBA.Foliage.Ground")))
        {
            continue;
        }
        // 一个关卡内可有多个显式锚点时，选择离当前舞台最近的平面，避免迭代顺序决定结果。
        const double DistanceSquared = FVector::DistSquared2D(
            It->GetActorLocation(), Origin);
        if (DistanceSquared < NearestDistanceSquared)
        {
            NearestDistanceSquared = DistanceSquared;
            GroundZ = It->GetActorLocation().Z;
            bFoundGroundAnchor = true;
        }
    }
    if (!bFoundGroundAnchor)
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Warning,
            TEXT("角色预览工作室缺少DBA.Foliage.Ground锚点，落叶暂使用舞台高度近似地面。"));
    }

    // Landscape（地形）表面可能高于锚点的固定Z值。进入预览时针对角色脚下与周边
    // 执行3x3次静态地面查询（只执行一次，不在Tick逐帧检测），取局部最高可见面作为
    // 贴地视觉参考，避免原有固定Z导致花瓣进入不平整地表。
    // 这些射线只允许命中当前工作室所属Level内的物体，且排除角色舞台自身。
    // 桃花已校准为约2至3厘米且触地贴平；沿用5厘米间隙会让花瓣明显悬浮。
    // 0.25厘米仅防止表面深度冲突；地面模块另保留0.25厘米，不作为玩法碰撞高度。
    constexpr float GroundClearanceCm = 0.25f;
    constexpr double SampleStepCm = 230.0;  // 限定近景角色周边，不让远山地形抬高全部花瓣。
    FCollisionQueryParams CollisionParams(SCENE_QUERY_STAT(DivineBeastsPreviewFoliageGround), false);
    CollisionParams.AddIgnoredActor(PreviewStage.Get());
    int32 ValidLandscapeHits = 0;
    double LocalMaxZ = GroundZ;
    for (int32 X = -1; X <= 1; ++X)
    {
        for (int32 Y = -1; Y <= 1; ++Y)
        {
            const FVector Start(Origin.X + X * SampleStepCm,
                Origin.Y + Y * SampleStepCm, Origin.Z + 2200.0);
            const FVector End(Start.X, Start.Y, Origin.Z - 2200.0);
            FHitResult Hit;
            if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, CollisionParams))
                continue;
            const AActor* SurfaceActor = Hit.GetActor();
            if (!IsValid(SurfaceActor) || SurfaceActor->GetLevel() != OwnedLevel ||
                Hit.ImpactPoint.ContainsNaN())
                continue;
            LocalMaxZ = FMath::Max(LocalMaxZ, Hit.ImpactPoint.Z);
            ++ValidLandscapeHits;
        }
    }
    // 即使Landscape没有开启Visibility碰撞，也保留锚点退化路径。
    // 仅增加视觉高度；不会移动角色、改变地形或产生服务器物理事实。
    GroundZ = static_cast<float>(LocalMaxZ + GroundClearanceCm);
    UE_LOG(LogDivineBeastsCharacterPreview, Verbose,
        TEXT("预览自然飘落地表：采样=%d，落地安全高度=%.2fcm，锚点=%s"),
        ValidLandscapeHits, GroundZ, bFoundGroundAnchor ? TEXT("有效") : TEXT("退化"));

    FString Error;
    if (!Presentation->StartFrontEndFoliage(
            PreviewFoliageStyle, Origin, GroundZ, Error) && !Error.IsEmpty())
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Warning,
            TEXT("前端环境落叶未启用：%s"), *Error);
    }
    // 鸟群与落叶独立加载；使用地面采样高度指定远景安全空域，不遮挡英雄主体。
    FString BirdError;
    const FVector BirdOrigin(Origin.X + 1800.0, Origin.Y + 1800.0, GroundZ + 800.0);
    if (!Presentation->StartAmbientBirdFlock(TEXT("DBA.FrontEnd.CharacterStudio.Birds"),
            TEXT("Seagull"), 12, BirdOrigin, BirdError) && !BirdError.IsEmpty())
        UE_LOG(LogDivineBeastsCharacterPreview, Warning, TEXT("前端鸟群未启用：%s"), *BirdError);
}

void UDivineBeastsCharacterPreviewSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
        this,
        &UDivineBeastsCharacterPreviewSubsystem::HandleWorldCleanup);
}

void UDivineBeastsCharacterPreviewSubsystem::Deinitialize()
{
    if (WorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
        WorldCleanupHandle.Reset();
    }

    // 本地玩家销毁属于退出清理，不等待可能已经结束的游戏世界计时器。
    DeactivatePreviewSceneInternal(false);
    Super::Deinitialize();
}

bool UDivineBeastsCharacterPreviewSubsystem::ActivatePreviewScene()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return false;
    }

    // 用户在退出混合期间立即重进时保留当前舞台，并取消过期卸载任务。
    if (bPreviewDeactivationPending)
    {
        World->GetTimerManager().ClearTimer(DeferredPreviewTeardownHandle);
        bPreviewDeactivationPending = false;
    }

    ResolvePreviewStage();
    if (PreviewStage.IsValid())
    {
        BindPreviewCamera();
        EnsurePreviewFoliage();
        TryApplyPendingAppearance();
        return true;
    }

    if (PreviewStreamingLevel)
    {
        return true;
    }

    bool bSuccess = false;
    PreviewStreamingLevel =
        ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
            this,
            CharacterStudioWorld,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            bSuccess);
    if (!bSuccess || !PreviewStreamingLevel)
    {
        PreviewStreamingLevel = nullptr;
        return false;
    }

    PreviewStreamingLevel->OnLevelShown.AddDynamic(
        this,
        &UDivineBeastsCharacterPreviewSubsystem::HandlePreviewLevelShown);
    return true;
}

void UDivineBeastsCharacterPreviewSubsystem::DeactivatePreviewScene()
{
    DeactivatePreviewSceneInternal(true);
}

void UDivineBeastsCharacterPreviewSubsystem::DeactivatePreviewSceneInternal(
    bool bAllowCameraBlend)
{
    if (bPreviewDeactivationPending && bAllowCameraBlend)
    {
        return;
    }

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (auto* Presentation = LocalPlayer->GetSubsystem<UDivineBeastsPresentationClientSubsystem>())
        {
            Presentation->StopFrontEndFoliage();
            Presentation->StopAmbientBirdFlock(TEXT("DBA.FrontEnd.CharacterStudio.Birds"));
        }
    }
    CancelPendingLoads();
    RequestedHeroDefinitionId = NAME_None;
    PendingProfile = nullptr;
    DevelopmentDynamicMaterials.Reset();

    // 默认观测球体只恢复本次借用实例的原有可见性。
    if (ADefaultPawn* Observer = HiddenPreviewObserver.Get())
    {
        Observer->SetActorHiddenInGame(bObserverWasHidden);
    }
    HiddenPreviewObserver.Reset();

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
    const float BlendTime = PreviewStage.IsValid()
        ? FMath::Clamp(PreviewStage->CameraBlendSeconds, 0.0f, 2.0f)
        : 0.0f;
    bool bRestoreBlendStarted = false;

    if (bAllowCameraBlend && LocalPlayer && PreviewStage.IsValid())
    {
        EnsurePreviewCameraProfile();
        if (PreviewCameraProfile)
        {
            PreviewCameraProfile->BlendTimeSeconds = BlendTime;
        }
        if (UGamePlatformCinematicCameraSubsystem* CameraSubsystem =
                LocalPlayer->GetSubsystem<UGamePlatformCinematicCameraSubsystem>())
        {
            // 插件仅在本地预览仍拥有ViewTarget时恢复；若其他系统已抢占则返回false。
            bRestoreBlendStarted = CameraSubsystem->EndCinematicView();
        }
    }

    if (bRestoreBlendStarted && BlendTime > 0.0f && World && !World->bIsTearingDown)
    {
        // 保留镜头源及预览外观，直到PlayerCameraManager结束混合后再卸载流送关卡。
        bPreviewDeactivationPending = true;
        World->GetTimerManager().SetTimer(
            DeferredPreviewTeardownHandle,
            this,
            &UDivineBeastsCharacterPreviewSubsystem::CompletePreviewSceneDeactivation,
            BlendTime + 0.05f,
            false);
        return;
    }

    CompletePreviewSceneDeactivation();
}

void UDivineBeastsCharacterPreviewSubsystem::CompletePreviewSceneDeactivation()
{
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UWorld* World = LocalPlayer->GetWorld())
        {
            World->GetTimerManager().ClearTimer(DeferredPreviewTeardownHandle);
        }
    }
    bPreviewDeactivationPending = false;

    // 退出混合已经结束或相机所有权已被抢占，恢复舞台初始FOV，避免前端配置残留。
    if (PreviewStage.IsValid() &&
        CameraFovOwnerStage.Get() == PreviewStage.Get() &&
        bHasCameraFieldOfViewSnapshot)
    {
        if (UCameraComponent* Camera = PreviewStage->GetPreviewCamera())
        {
            Camera->SetFieldOfView(CameraFieldOfViewBeforePreview);
        }
    }
    CameraFovOwnerStage.Reset();
    bHasCameraFieldOfViewSnapshot = false;

    if (PreviewStage.IsValid())
    {
        PreviewStage->ClearPreviewAppearance();
    }
    PreviewStage.Reset();

    if (PreviewStreamingLevel)
    {
        PreviewStreamingLevel->OnLevelShown.RemoveDynamic(
            this,
            &UDivineBeastsCharacterPreviewSubsystem::HandlePreviewLevelShown);
        PreviewStreamingLevel->SetShouldBeVisible(false);
        PreviewStreamingLevel->SetShouldBeLoaded(false);
        PreviewStreamingLevel->SetIsRequestingUnloadAndRemoval(true);
        PreviewStreamingLevel = nullptr;
    }
}

bool UDivineBeastsCharacterPreviewSubsystem::PreviewHero(
    FName HeroDefinitionId)
{
    if (HeroDefinitionId.IsNone() || !ActivatePreviewScene())
    {
        return false;
    }

    if (RequestedHeroDefinitionId == HeroDefinitionId &&
        (PendingProfile || PreviewStage.IsValid()))
    {
        return true;
    }

    CancelPendingLoads();
    PendingProfile = nullptr;
    DevelopmentDynamicMaterials.Reset();
    RequestedHeroDefinitionId = HeroDefinitionId;

    const int32 ExpectedGeneration = ++RequestGeneration;
    const TWeakObjectPtr<UDivineBeastsCharacterPreviewSubsystem> WeakThis(this);
    ProfileLease =
        FDivineBeastsCharacterAppearanceCatalog::RequestDefaultProfile(
            HeroDefinitionId,
            [WeakThis, HeroDefinitionId, ExpectedGeneration](
                UDivineBeastsCharacterAppearanceProfile* Profile)
            {
                if (WeakThis.IsValid())
                {
                    WeakThis->HandleProfileLoaded(
                        Profile,
                        HeroDefinitionId,
                        ExpectedGeneration);
                }
            });
    return true;
}

void UDivineBeastsCharacterPreviewSubsystem::ClearPreview()
{
    CancelPendingLoads();
    RequestedHeroDefinitionId = NAME_None;
    PendingProfile = nullptr;
    DevelopmentDynamicMaterials.Reset();
    if (PreviewStage.IsValid())
    {
        PreviewStage->ClearPreviewAppearance();
    }
}

void UDivineBeastsCharacterPreviewSubsystem::RotatePreview(
    float DeltaYawDegrees)
{
    if (PreviewStage.IsValid())
    {
        PreviewStage->AddPreviewYaw(DeltaYawDegrees);
    }
}

void UDivineBeastsCharacterPreviewSubsystem::SetPreviewCameraDistance(
    float DistanceCentimeters)
{
    if (!FMath::IsFinite(DistanceCentimeters) || !PreviewStage.IsValid())
    {
        return;
    }

    PreviewStage->SetCameraDistance(DistanceCentimeters);
    EnsurePreviewCameraProfile();
    if (PreviewCameraProfile)
    {
        PreviewCameraProfile->FocusDistanceCm = PreviewStage->GetCameraDistanceCentimeters();
    }
}

bool UDivineBeastsCharacterPreviewSubsystem::AdjustPreviewCameraDistance(
    float DeltaCentimeters)
{
    if (!FMath::IsFinite(DeltaCentimeters) || !PreviewStage.IsValid())
    {
        return false;
    }

    SetPreviewCameraDistance(
        PreviewStage->GetCameraDistanceCentimeters() + DeltaCentimeters);
    return true;
}

bool UDivineBeastsCharacterPreviewSubsystem::SetPreviewCameraFocalLength(
    float FocalLengthMm)
{
    if (!FMath::IsFinite(FocalLengthMm))
    {
        return false;
    }

    EnsurePreviewCameraProfile();
    if (!PreviewCameraProfile)
    {
        return false;
    }

    PreviewCameraProfile->FocalLengthMm = FMath::Clamp(FocalLengthMm, 24.0f, 85.0f);
    return !PreviewStage.IsValid() || ApplyPreviewCameraProfile();
}

bool UDivineBeastsCharacterPreviewSubsystem::AdjustPreviewCameraFocalLength(
    float DeltaFocalLengthMm)
{
    if (!FMath::IsFinite(DeltaFocalLengthMm))
    {
        return false;
    }
    return SetPreviewCameraFocalLength(
        GetPreviewCameraFocalLength() + DeltaFocalLengthMm);
}

float UDivineBeastsCharacterPreviewSubsystem::GetPreviewCameraDistance() const
{
    return PreviewStage.IsValid()
        ? PreviewStage->GetCameraDistanceCentimeters()
        : 320.0f;
}

float UDivineBeastsCharacterPreviewSubsystem::GetPreviewCameraFocalLength() const
{
    return PreviewCameraProfile
        ? PreviewCameraProfile->FocalLengthMm
        : 35.0f;
}

void UDivineBeastsCharacterPreviewSubsystem::HandlePreviewLevelShown()
{
    ResolvePreviewStage();
    BindPreviewCamera();
    TryApplyPendingAppearance();
    // 预览关卡已显示后才绑定该世界的落叶表现，不能在清理函数签名内调用。
    EnsurePreviewFoliage();
}

void UDivineBeastsCharacterPreviewSubsystem::HandleWorldCleanup(
    UWorld* World,
    bool,
    bool)
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (World && LocalPlayer && LocalPlayer->GetWorld() == World)
    {
        World->GetTimerManager().ClearTimer(DeferredPreviewTeardownHandle);
        bPreviewDeactivationPending = false;
        CancelPendingLoads();
        PreviewStreamingLevel = nullptr;
        PreviewStage.Reset();
        CameraFovOwnerStage.Reset();
        bHasCameraFieldOfViewSnapshot = false;
        // 世界销毁时原观测Pawn随世界退出，不向新世界实例恢复旧可见性。
        HiddenPreviewObserver.Reset();
        PendingProfile = nullptr;
        DevelopmentDynamicMaterials.Reset();
    }
}

void UDivineBeastsCharacterPreviewSubsystem::ResolvePreviewStage()
{
    if (PreviewStage.IsValid())
    {
        return;
    }

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
    if (!World)
    {
        return;
    }

    // 流送卸载是异步的：刚退出的关卡Actor仍可能出现在世界迭代器里。
    // 只接受当前实例已显示的关卡或显式打开的持久工作室；拒绝旧卸载关卡，
    // 避免退出后立即重进绑定一个即将销毁的舞台，并隔离其他本地玩家实例。
    ULevel* OwnedLevel = PreviewStreamingLevel && PreviewStreamingLevel->IsLevelVisible()
        ? PreviewStreamingLevel->GetLoadedLevel() : nullptr;
    for (TActorIterator<AGamePlatformCharacterPreviewStage> It(World); It; ++It)
    {
        if (It->GetLevel() != World->PersistentLevel && It->GetLevel() != OwnedLevel)
        {
            continue;
        }
        PreviewStage = *It;
        break;
    }
}

void UDivineBeastsCharacterPreviewSubsystem::EnsurePreviewCameraProfile()
{
    if (PreviewCameraProfile)
    {
        return;
    }

    PreviewCameraProfile = NewObject<UGamePlatformCinematicCameraProfile>(
        this, TEXT("CharacterPreviewCameraProfile"), RF_Transient);
    if (!PreviewCameraProfile)
    {
        return;
    }

    PreviewCameraProfile->Mode = EGamePlatformCameraPresentationMode::CharacterPreview;
    PreviewCameraProfile->SensorWidthMm = 36.0f;
    PreviewCameraProfile->FocalLengthMm = 35.0f;
    PreviewCameraProfile->Aperture = 5.6f;
    PreviewCameraProfile->FocusDistanceCm = 320.0f;
    PreviewCameraProfile->BlendTimeSeconds = 0.45f;
    // 正式角色选择保持清晰、稳定；不对场景角色启用浅景深追焦。
    PreviewCameraProfile->bEnableCinematicFocus = false;
}

bool UDivineBeastsCharacterPreviewSubsystem::ApplyPreviewCameraProfile()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!LocalPlayer || !PreviewStage.IsValid() || !PreviewCameraProfile)
    {
        return false;
    }

    UGamePlatformCinematicCameraSubsystem* CameraSubsystem =
        LocalPlayer->GetSubsystem<UGamePlatformCinematicCameraSubsystem>();
    UCameraComponent* PreviewCamera = PreviewStage->GetPreviewCamera();
    if (!CameraSubsystem || !PreviewCamera)
    {
        return false;
    }

    if (CameraFovOwnerStage.Get() != PreviewStage.Get())
    {
        CameraFovOwnerStage = PreviewStage;
        bHasCameraFieldOfViewSnapshot = false;
    }
    const float OriginalFieldOfView = PreviewCamera->FieldOfView;
    PreviewCameraProfile->BlendTimeSeconds =
        FMath::Clamp(PreviewStage->CameraBlendSeconds, 0.0f, 2.0f);
    PreviewCameraProfile->FocusDistanceCm =
        PreviewStage->GetCameraDistanceCentimeters();

    if (!CameraSubsystem->BeginCinematicView(PreviewStage.Get(), PreviewCameraProfile))
    {
        return false;
    }

    if (!bHasCameraFieldOfViewSnapshot)
    {
        CameraFieldOfViewBeforePreview = OriginalFieldOfView;
        bHasCameraFieldOfViewSnapshot = true;
    }
    return true;
}

void UDivineBeastsCharacterPreviewSubsystem::BindPreviewCamera()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!LocalPlayer || !PreviewStage.IsValid())
    {
        return;
    }

    EnsurePreviewCameraProfile();
    if (!ApplyPreviewCameraProfile())
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Warning,
            TEXT("正式角色预览相机未能取得本地镜头所有权；保留当前游戏/登录视角。"));
        return;
    }

    // 只有成功借用预览镜头后才隐藏默认观测Pawn；失败时保留登录/游戏原画面。
    if (!HiddenPreviewObserver.IsValid())
    {
        if (APlayerController* Controller =
                LocalPlayer->GetPlayerController(LocalPlayer->GetWorld()))
        {
            if (ADefaultPawn* Observer = Cast<ADefaultPawn>(Controller->GetPawn());
                Observer && Observer->IsLocallyControlled())
            {
                bObserverWasHidden = Observer->IsHidden();
                HiddenPreviewObserver = Observer;
                Observer->SetActorHiddenInGame(true);
            }
        }
    }
}

void UDivineBeastsCharacterPreviewSubsystem::CancelPendingLoads()
{
    ++RequestGeneration;

    if (ProfileLease.IsValid())
    {
        FGamePlatformAssetLoader::Cancel(ProfileLease);
        ProfileLease.Reset();
    }
    if (VisualLease.IsValid())
    {
        FGamePlatformAssetLoader::Cancel(VisualLease);
        VisualLease.Reset();
    }
}

void UDivineBeastsCharacterPreviewSubsystem::HandleProfileLoaded(
    UDivineBeastsCharacterAppearanceProfile* Profile,
    FName ExpectedHeroDefinitionId,
    int32 ExpectedRequestGeneration)
{
    UE_LOG(LogDivineBeastsCharacterPreview, Display, TEXT("Profile callback: Hero=%s Generation=%d/%d Profile=%s"),
        *ExpectedHeroDefinitionId.ToString(), ExpectedRequestGeneration, RequestGeneration, *GetNameSafe(Profile));
    if (ExpectedRequestGeneration != RequestGeneration ||
        ExpectedHeroDefinitionId != RequestedHeroDefinitionId)
    {
        return;
    }

    ProfileLease.Reset();
    if (!Profile || Profile->HeroDefinitionId != ExpectedHeroDefinitionId)
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Warning, TEXT("Preview profile missing or hero identity mismatch."));
        return;
    }

    FString Error;
    if (!Profile->IsProfileValid(Error))
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Warning, TEXT("Invalid preview profile: %s"), *Error);
        return;
    }

    PendingProfile = Profile;

    TArray<FSoftObjectPath> Assets;
    if (!Profile->SkeletalMesh.IsNull())
    {
        Assets.Add(Profile->SkeletalMesh.ToSoftObjectPath());
    }
    for (const TSoftObjectPtr<UMaterialInterface>& Material :
         Profile->MaterialOverrides)
    {
        if (!Material.IsNull())
        {
            Assets.AddUnique(Material.ToSoftObjectPath());
        }
    }
    if (!Profile->AnimInstanceClass.IsNull())
    {
        Assets.AddUnique(Profile->AnimInstanceClass.ToSoftObjectPath());
    }
    else if (Profile->bDevelopmentPlaceholder)
    {
        // 使用同一异步资源租约，取消或切换英雄时仍由既有代次栅栏丢弃迟到回调。
        Assets.AddUnique(DevelopmentPreviewIdleClass.ToSoftObjectPath());
    }

    const TWeakObjectPtr<UDivineBeastsCharacterPreviewSubsystem> WeakThis(this);
    VisualLease = FGamePlatformAssetLoader::RequestAsyncLoad(
        Assets,
        FStreamableDelegate::CreateLambda(
            [WeakThis, Profile, ExpectedHeroDefinitionId, ExpectedRequestGeneration]()
            {
                if (WeakThis.IsValid())
                {
                    WeakThis->HandleVisualResourcesLoaded(
                        Profile,
                        ExpectedHeroDefinitionId,
                        ExpectedRequestGeneration);
                }
            }));

    // 全部资源已常驻时加载器可能同步完成，统一尝试一次幂等应用。
    TryApplyPendingAppearance();
}

void UDivineBeastsCharacterPreviewSubsystem::HandleVisualResourcesLoaded(
    UDivineBeastsCharacterAppearanceProfile* Profile,
    FName ExpectedHeroDefinitionId,
    int32 ExpectedRequestGeneration)
{
    if (ExpectedRequestGeneration != RequestGeneration ||
        ExpectedHeroDefinitionId != RequestedHeroDefinitionId ||
        PendingProfile != Profile)
    {
        return;
    }

    VisualLease.Reset();
    TryApplyPendingAppearance();
}

bool UDivineBeastsCharacterPreviewSubsystem::TryApplyPendingAppearance()
{
    if (!PreviewStage.IsValid() || !PendingProfile ||
        PendingProfile->HeroDefinitionId != RequestedHeroDefinitionId)
    {
        return false;
    }

    USkeletalMesh* Mesh = PendingProfile->SkeletalMesh.Get();
    if (!Mesh)
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Verbose, TEXT("Preview mesh is not loaded: %s"), *PendingProfile->SkeletalMesh.ToSoftObjectPath().ToString());
        return false;
    }

    TArray<UMaterialInterface*> Materials;
    Materials.Reserve(PendingProfile->MaterialOverrides.Num());
    for (const TSoftObjectPtr<UMaterialInterface>& Material :
         PendingProfile->MaterialOverrides)
    {
        UMaterialInterface* Resolved = Material.Get();
        if (!Material.IsNull() && !Resolved)
        {
            return false;
        }
        Materials.Add(Resolved);
    }

    UClass* AnimClass = PendingProfile->AnimInstanceClass.IsNull()
        ? (PendingProfile->bDevelopmentPlaceholder ? DevelopmentPreviewIdleClass.Get() : nullptr)
        : PendingProfile->AnimInstanceClass.Get();
    if ((!PendingProfile->AnimInstanceClass.IsNull() || PendingProfile->bDevelopmentPlaceholder) && !AnimClass)
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Warning, TEXT("Required preview animation class is not loaded: Hero=%s"), *RequestedHeroDefinitionId.ToString());
        return false;
    }

    FString SkeletonError;
    if (!UDivineBeastsCharacterAppearanceProfile::ValidateLoadedAnimationClass(Mesh, AnimClass, SkeletonError))
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Warning, TEXT("Preview mesh/animation skeleton rejected: Hero=%s AnimClass=%s Reason=%s"),
            *RequestedHeroDefinitionId.ToString(), *GetNameSafe(AnimClass), *SkeletonError);
        return false;
    }

    if (!PreviewStage->ApplyPreviewAppearance(
            Mesh,
            Materials,
            TSubclassOf<UAnimInstance>(AnimClass),
            PendingProfile->MeshRelativeLocation,
            PendingProfile->MeshRelativeRotation,
            PendingProfile->MeshRelativeScale))
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Warning, TEXT("Preview stage rejected appearance: Hero=%s"), *RequestedHeroDefinitionId.ToString());
        return false;
    }

    UE_LOG(LogDivineBeastsCharacterPreview, Display, TEXT("Preview appearance applied: Hero=%s Mesh=%s AnimClass=%s"),
        *RequestedHeroDefinitionId.ToString(), *GetNameSafe(Mesh), *GetNameSafe(AnimClass));

    DevelopmentDynamicMaterials.Reset();
    if (PendingProfile->bDevelopmentPlaceholder)
    {
        USkeletalMeshComponent* MeshComponent =
            PreviewStage->GetPreviewMeshComponent();
        const FName TintParameterName =
            PendingProfile->MaterialOverrides.IsEmpty()
                ? FName(TEXT("Paint Tint"))
                : FName(TEXT("PrototypeTint"));
        if (MeshComponent)
        {
            DevelopmentDynamicMaterials.Reserve(
                MeshComponent->GetNumMaterials());
            for (int32 Index = 0;
                 Index < MeshComponent->GetNumMaterials();
                 ++Index)
            {
                UMaterialInterface* BaseMaterial =
                    MeshComponent->GetMaterial(Index);
                if (!BaseMaterial)
                {
                    continue;
                }

                UMaterialInstanceDynamic* DynamicMaterial =
                    UMaterialInstanceDynamic::Create(BaseMaterial, this);
                if (!DynamicMaterial)
                {
                    continue;
                }

                DynamicMaterial->SetVectorParameterValue(
                    TintParameterName,
                    PendingProfile->DevelopmentTint);
                MeshComponent->SetMaterial(Index, DynamicMaterial);
                DevelopmentDynamicMaterials.Add(DynamicMaterial);
            }
        }
    }

    return true;
}

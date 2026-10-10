// 本文件属于GamePlatform平台层 GamePlatformVFX，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#include "Integration/Presentation/GamePlatformVFXPresentationProvider.h"
#include "Interfaces/GamePlatformVFXService.h"
#include "Types/GamePlatformVFXRequest.h"
#include "Engine/World.h"
#include "HAL/PlatformProperties.h"

// 构造弱引用的UWorld->UObject转换放在有完整世界定义的实现文件，私有头只保留声明。
FGamePlatformVFXPresentationProvider::FGamePlatformVFXPresentationProvider(UWorld* InWorld)
    : World(InWorld)
{
}

bool FGamePlatformVFXPresentationProvider::Handle(
    const FGamePlatformPresentationRequest& Request) const
{
    if (!Request.IsValid() || Request.ProviderChannel != TEXT("VFX"))
    {
        return false;
    }

    IGamePlatformVFXService* Service = IGamePlatformVFXService::Get(World.Get());
    if (!Service)
    {
        return false;
    }

    FGamePlatformVFXRequest VFXRequest;
    VFXRequest.RequestId = Request.RequestId;
    VFXRequest.ActivationId = Request.RequestId;
    VFXRequest.PredictionKey = Request.RequestGeneration;
    VFXRequest.SemanticTag = Request.SemanticTag;
    VFXRequest.ContextId = Request.ContextId;
    VFXRequest.HeroDefinitionId = Request.Context.HeroDefinitionId;
    VFXRequest.AbilityId = Request.Context.AbilityId;
    VFXRequest.SkinId = Request.Context.SkinId;
    VFXRequest.WorldId = Request.Context.WorldId;
    VFXRequest.ContextTags = Request.ContextTags;
    VFXRequest.DefinitionId = Request.DefinitionId;
    VFXRequest.PlatformId = FName(FPlatformProperties::IniPlatformName());
    VFXRequest.SpawnContext.Location = Request.SourceLocation;
    VFXRequest.SpawnContext.TargetLocation = Request.TargetLocation;
    VFXRequest.SpawnContext.ImpactLocation = Request.ImpactLocation;
    VFXRequest.SpawnContext.ImpactNormal = Request.ImpactNormal;

    switch (Request.Priority)
    {
    case EGamePlatformPresentationPriority::Critical:
        VFXRequest.Importance = EGamePlatformVFXImportance::Critical;
        break;
    case EGamePlatformPresentationPriority::High:
        VFXRequest.Importance = EGamePlatformVFXImportance::Combat;
        break;
    case EGamePlatformPresentationPriority::Normal:
        VFXRequest.Importance = EGamePlatformVFXImportance::Status;
        break;
    case EGamePlatformPresentationPriority::Low:
    default:
        VFXRequest.Importance = EGamePlatformVFXImportance::Ambient;
        break;
    }

    switch (Request.PredictionState)
    {
    case EGamePlatformPresentationPredictionState::Predicted:
        VFXRequest.PredictionState = EGamePlatformVFXPredictionState::Predicted;
        break;
    case EGamePlatformPresentationPredictionState::Cancelled:
        VFXRequest.PredictionState = EGamePlatformVFXPredictionState::Cancelled;
        break;
    case EGamePlatformPresentationPredictionState::Confirmed:
        VFXRequest.PredictionState = EGamePlatformVFXPredictionState::Confirmed;
        break;
    case EGamePlatformPresentationPredictionState::Corrected:
        VFXRequest.PredictionState = EGamePlatformVFXPredictionState::Corrected;
        break;
    default:
        VFXRequest.PredictionState = EGamePlatformVFXPredictionState::Confirmed;
        break;
    }

    const auto Result = Service->Play(VFXRequest);
    return Result.IsAccepted() || (VFXRequest.PredictionState == EGamePlatformVFXPredictionState::Cancelled && Result.Code == EGamePlatformVFXResultCode::Cancelled);
}

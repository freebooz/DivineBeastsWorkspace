#include "Policy/GamePlatformSFXPolicy.h"

#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Types/GamePlatformId.h"

FGamePlatformResult FGamePlatformSFXPolicy::ValidateRequest(
    const FGamePlatformSFXRequest& Request)
{
    if (Request.DefinitionId.IsNone())
    {
        return FGamePlatformResult::Failure(
            TEXT("SFX.DefinitionIdMissing"),
            TEXT("SFX播放请求缺少逻辑DefinitionId。"));
    }

    if (Request.Location.ContainsNaN() || Request.Rotation.ContainsNaN())
    {
        return FGamePlatformResult::Failure(
            TEXT("SFX.TransformInvalid"),
            TEXT("SFX播放请求包含非法空间坐标或旋转。"));
    }

    if (!FMath::IsFinite(Request.VolumeMultiplier) || Request.VolumeMultiplier < 0.0f || Request.VolumeMultiplier > 4.0f ||
        !FMath::IsFinite(Request.PitchMultiplier) || Request.PitchMultiplier < 0.25f || Request.PitchMultiplier > 4.0f ||
        !FMath::IsFinite(Request.StartTimeSeconds) || Request.StartTimeSeconds < 0.0f)
    {
        return FGamePlatformResult::Failure(
            TEXT("SFX.RequestRangeInvalid"),
            TEXT("SFX播放请求的音量、音高或起播时间非法。"));
    }

    for (const TPair<FName, float>& Pair : Request.FloatParameters)
    {
        if (Pair.Key.IsNone() || !FMath::IsFinite(Pair.Value))
        {
            return FGamePlatformResult::Failure(
                TEXT("SFX.RequestParameterInvalid"),
                TEXT("SFX请求浮点参数名称不能为空且数值必须有限。"));
        }
    }

    FPrimaryAssetId Unused;
    return BuildDefinitionAssetId(Request.DefinitionId, Unused);
}

FGamePlatformResult FGamePlatformSFXPolicy::BuildDefinitionAssetId(
    const FName DefinitionId,
    FPrimaryAssetId& OutAssetId)
{
    // 复用GamePlatformData（平台数据模块）的唯一定义身份转换。
    // 保留SFX原有两类错误消息；资源实际加载仍由平台数据租约拥有。
    const EGamePlatformDefinitionIdBuildStatus Status =
        UGamePlatformPrimaryDataAsset::BuildDefinitionAssetId(DefinitionId, OutAssetId);
    if (Status == EGamePlatformDefinitionIdBuildStatus::InvalidLogicalId)
    {
        return FGamePlatformResult::Failure(
            TEXT("SFX.DefinitionIdInvalid"),
            TEXT("SFX DefinitionId必须是namespace.name@version格式的GamePlatform逻辑身份，不能使用资产路径。"));
    }
    if (Status == EGamePlatformDefinitionIdBuildStatus::EmptyCanonicalId)
    {
        return FGamePlatformResult::Failure(
            TEXT("SFX.DefinitionIdInvalid"),
            TEXT("SFX DefinitionId规范化失败。"));
    }
    return FGamePlatformResult::Success();
}

// 通用表面材质资产路径与MPC参数契约唯一实现。
#include "Validation/GamePlatformSurfaceAssetContract.h"

#include "Materials/MaterialParameterCollection.h"
#include "Types/GamePlatformSurfaceParameterNames.h"

const FString& FGamePlatformSurfaceAssetContract::GetGlobalParameterCollectionPackageName()
{
    static const FString PackageName(TEXT("/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal"));
    return PackageName;
}

FSoftObjectPath FGamePlatformSurfaceAssetContract::GetGlobalParameterCollectionObjectPath()
{
    return FSoftObjectPath(
        TEXT("/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal.MPC_GP_SurfaceGlobal"));
}

const TArray<TPair<FName, float>>& FGamePlatformSurfaceAssetContract::GetRequiredScalarParameters()
{
    static const TArray<TPair<FName, float>> Parameters =
    {
        { GamePlatformSurfaceParameters::GlobalWetness, 0.0f },
        { GamePlatformSurfaceParameters::GlobalSnowAmount, 0.0f },
        { GamePlatformSurfaceParameters::GlobalSnowHeightCm, 0.0f },
        { GamePlatformSurfaceParameters::GlobalMossInfluence, 1.0f },
        { GamePlatformSurfaceParameters::GlobalPuddleAmount, 0.0f },
        { GamePlatformSurfaceParameters::RainIntensity, 0.0f },
        { GamePlatformSurfaceParameters::SnowIntensity, 0.0f },
        { GamePlatformSurfaceParameters::TemperatureCelsius, 20.0f },
        { GamePlatformSurfaceParameters::GlobalWindSeed, 2026.0f }
    };
    return Parameters;
}

const TArray<TPair<FName, FLinearColor>>& FGamePlatformSurfaceAssetContract::GetRequiredVectorParameters()
{
    static const TArray<TPair<FName, FLinearColor>> Parameters =
    {
        { GamePlatformSurfaceParameters::GlobalWindFrom, FLinearColor(0, 0, 0, 0) },
        { GamePlatformSurfaceParameters::GlobalWindTo, FLinearColor(0, 0, 0, 0) },
        { GamePlatformSurfaceParameters::GlobalWindTiming, FLinearColor(0, 0, 0, 0) }
    };
    return Parameters;
}

const TArray<FString>& FGamePlatformSurfaceAssetContract::GetRequiredMaterialAuthoringPackages()
{
    static const TArray<FString> Packages =
    {
        TEXT("/GamePlatformSurface/Materials/M_GP_Surface_Master"),
        TEXT("/GamePlatformSurface/Materials/M_GP_Surface_Lite"),
        TEXT("/GamePlatformSurface/MaterialFunctions/Masks/MF_GP_SlopeMask"),
        TEXT("/GamePlatformSurface/MaterialFunctions/Masks/MF_GP_HeightMask"),
        TEXT("/GamePlatformSurface/MaterialFunctions/Utility/MF_GP_WorldNoise"),
        TEXT("/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_SnowLayer"),
        TEXT("/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_MossLayer"),
        TEXT("/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_WetnessLayer"),
        TEXT("/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_PuddleLayer")
    };
    return Packages;
}

const TArray<FString>& FGamePlatformSurfaceAssetContract::GetOptionalMaterialAuthoringPackages()
{
    static const TArray<FString> Packages =
    {
        // 写实积雪属于可选增强材质；不提升为全部游戏项目的必需资源。
        TEXT("/GamePlatformSurface/Materials/M_GP_SnowCover_Detailed"),
        // 苔藓—积雪PBR可选增强材质，不能强迫所有项目引用。
        TEXT("/GamePlatformSurface/Materials/M_GP_Surface_SnowMoss_PBR"),
        TEXT("/GamePlatformSurface/Materials/M_GP_Surface_Landscape"),
        TEXT("/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_DirtLayer"),
        TEXT("/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_DustLayer"),
        TEXT("/GamePlatformSurface/MaterialFunctions/Utility/MF_GP_MacroVariation"),
        TEXT("/GamePlatformSurface/MaterialFunctions/Utility/MF_GP_NormalBlend")
    };
    return Packages;
}

bool FGamePlatformSurfaceAssetContract::ValidateGlobalParameterCollection(
    const UMaterialParameterCollection& Collection,
    FString& OutError)
{
    OutError.Reset();

    const TArray<TPair<FName, float>>& Required = GetRequiredScalarParameters();
    if (Collection.ScalarParameters.Num() != Required.Num())
    {
        OutError = FString::Printf(
            TEXT("MPC标量参数数量不符：期望%d，实际%d。"),
            Required.Num(),
            Collection.ScalarParameters.Num());
        return false;
    }

    for (const TPair<FName, float>& Pair : Required)
    {
        // UE5.8 的 GetScalarParameterIndexByName 没有 ENGINE_API 导出，跨模块调用会产生链接错误。
        // 直接遍历公开参数数组，避免调用未导出的引擎符号；不影响运行时材质更新。
        const FCollectionScalarParameter* FoundParameter = Collection.ScalarParameters.FindByPredicate(
            [&Pair](const FCollectionScalarParameter& Candidate)
            {
                return Candidate.ParameterName == Pair.Key;
            });
        if (!FoundParameter)
        {
            OutError = FString::Printf(TEXT("MPC缺少标量参数：%s。"), *Pair.Key.ToString());
            return false;
        }

        if (!FMath::IsNearlyEqual(FoundParameter->DefaultValue, Pair.Value))
        {
            OutError = FString::Printf(
                TEXT("MPC参数%s默认值不符：期望%g，实际%g。"),
                *Pair.Key.ToString(),
                Pair.Value,
                FoundParameter->DefaultValue);
            return false;
        }
    }

    const auto& Vectors = GetRequiredVectorParameters();
    if (Collection.VectorParameters.Num() != Vectors.Num())
    {
        OutError = TEXT("Surface全局MPC必须包含统一风From/To/Timing三个向量。");
        return false;
    }
    for (const auto& Pair : Vectors)
    {
        const auto* Found = Collection.VectorParameters.FindByPredicate(
            [&Pair](const FCollectionVectorParameter& Value) { return Value.ParameterName == Pair.Key; });
        if (!Found || !Found->DefaultValue.Equals(Pair.Value))
        {
            OutError = FString::Printf(TEXT("统一风向量参数%s缺失或默认值不符。"), *Pair.Key.ToString());
            return false;
        }
    }

    return true;
}

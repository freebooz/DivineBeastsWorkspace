#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGSettings.h"
#include "Definitions/GamePlatformPCGEnvironmentDefinitions.h"
#include "GamePlatformPCGNodes.generated.h"

/** 所有首批点节点共用单点输入/输出合同；不额外创建模块。 */
UCLASS(Abstract, BlueprintType, ClassGroup=(Procedural))
class GAMEPLATFORMPCG_API UGamePlatformPCGPointNodeSettings : public UPCGSettings
{
    GENERATED_BODY()
protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return Super::DefaultPointInputPinProperties(); }
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class GAMEPLATFORMPCG_API UGamePlatformPCGWriteSchemaDefaultsSettings final : public UGamePlatformPCGPointNodeSettings
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("GP_WriteSchemaDefaults"); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("GamePlatformPCG", "WriteSchemaDefaults", "GamePlatform | PCG | Write Schema Defaults"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Classify", meta=(PCG_Overridable))
    FName DefaultLayerName = TEXT("Default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Classify", meta=(PCG_Overridable))
    int32 DefaultSeed = 1;

protected:
    virtual FPCGElementPtr CreateElement() const override;
};

class GAMEPLATFORMPCG_API FGamePlatformPCGWriteSchemaDefaultsElement final : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
    virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings*) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
};

/** WriteExclude（写排除属性）：把上游已确定的排除区域点写成统一Pcg.Exclude.*协议。 */
UCLASS(BlueprintType, ClassGroup=(Procedural))
class GAMEPLATFORMPCG_API UGamePlatformPCGWriteExcludeSettings final : public UGamePlatformPCGPointNodeSettings
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("GP_WriteExclude"); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("GamePlatformPCG", "WriteExclude", "GamePlatform | PCG | Write Exclude"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Classify", meta=(ClampMin="0.0", ClampMax="1.0", PCG_Overridable))
    float ExcludeStrength = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Classify", meta=(PCG_Overridable))
    FName ExcludeSource = TEXT("ManualLock");

protected:
    virtual FPCGElementPtr CreateElement() const override;
};

class GAMEPLATFORMPCG_API FGamePlatformPCGWriteExcludeElement final : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
    virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings*) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class GAMEPLATFORMPCG_API UGamePlatformPCGPriorityCarveSettings final : public UGamePlatformPCGPointNodeSettings
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("GP_PriorityCarve"); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("GamePlatformPCG", "PriorityCarve", "GamePlatform | PCG | Priority Carve"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
#endif

    /** 上游按优先级总线写入ExcludeMask后，本节点将达到阈值的点密度置0；不扫描World。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Resolve", meta=(ClampMin="0.0", ClampMax="1.0", PCG_Overridable))
    float ExcludeThreshold = 0.5f;

    /** 当前被切层优先级；通常来自PriorityTable（优先级表）解析后通过Graph Instance覆盖。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Resolve", meta=(ClampMin="0", ClampMax="100", PCG_Overridable))
    int32 SubjectPriority = 0;

    /** 排除来源优先级；必须严格高于SubjectPriority才允许挖洞。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Resolve", meta=(ClampMin="0", ClampMax="100", PCG_Overridable))
    int32 CarverPriority = 100;

protected:
    virtual FPCGElementPtr CreateElement() const override;
};

class GAMEPLATFORMPCG_API FGamePlatformPCGPriorityCarveElement final : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
    virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings*) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class GAMEPLATFORMPCG_API UGamePlatformPCGProjectAlignSettings final : public UGamePlatformPCGPointNodeSettings
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("GP_ProjectAlign"); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("GamePlatformPCG", "ProjectAlign", "GamePlatform | PCG | Project Align"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
#endif

    /** M0只负责对齐语义；真正地形投影由SG_ProjectOnLandscape（投影到地形子图）提供。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Resolve", meta=(PCG_Overridable))
    bool bKeepVertical = true;

protected:
    virtual FPCGElementPtr CreateElement() const override;
};

class GAMEPLATFORMPCG_API FGamePlatformPCGProjectAlignElement final : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
    virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings*) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class GAMEPLATFORMPCG_API UGamePlatformPCGApplySpawnPolicySettings final : public UGamePlatformPCGPointNodeSettings
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("GP_ApplySpawnPolicy"); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("GamePlatformPCG", "ApplySpawnPolicy", "GamePlatform | PCG | Apply Spawn Policy"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Resolve", meta=(ClampMin="0.0", ClampMax="1.0", PCG_Overridable))
    float DensityScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Resolve", meta=(ClampMin="0.01", PCG_Overridable))
    float UniformScale = 1.0f;

protected:
    virtual FPCGElementPtr CreateElement() const override;
};

class GAMEPLATFORMPCG_API FGamePlatformPCGApplySpawnPolicyElement final : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
    virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings*) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class GAMEPLATFORMPCG_API UGamePlatformPCGAssignMeshSetSettings final : public UGamePlatformPCGPointNodeSettings
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("GP_AssignMeshSet"); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("GamePlatformPCG", "AssignMeshSet", "GamePlatform | PCG | Assign MeshSet"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
#endif

    /** 这里只写稳定目录ID，不允许节点持有任意SoftPath（软路径）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Realize", meta=(PCG_Overridable))
    FName MeshSetId = NAME_None;

protected:
    virtual FPCGElementPtr CreateElement() const override;
};

class GAMEPLATFORMPCG_API FGamePlatformPCGAssignMeshSetElement final : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
    virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings*) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class GAMEPLATFORMPCG_API UGamePlatformPCGValidateSchemaSettings final : public UGamePlatformPCGPointNodeSettings
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("GP_ValidateSchema"); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("GamePlatformPCG", "ValidateSchema", "GamePlatform | PCG | Validate Schema"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
#endif

    /** 为空时验证Schema v1核心必需字段；额外字段必须同样已在Schema注册。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Debug")
    TArray<FName> RequiredAttributes;

protected:
    virtual FPCGElementPtr CreateElement() const override;
};

class GAMEPLATFORMPCG_API FGamePlatformPCGValidateSchemaElement final : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
    virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings*) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class GAMEPLATFORMPCG_API UGamePlatformPCGFitPostsToSplineSettings final : public UGamePlatformPCGPointNodeSettings
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("GP_FitPostsToSpline"); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("GamePlatformPCG", "FitPostsToSpline", "GamePlatform | PCG | Fit Posts To Spline"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Linear", meta=(ClampMin="1.0", PCG_Overridable))
    float PostSpacingCm = 200.0f;

protected:
    virtual FPCGElementPtr CreateElement() const override;
};

class GAMEPLATFORMPCG_API FGamePlatformPCGFitPostsToSplineElement final : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
    virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings*) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class GAMEPLATFORMPCG_API UGamePlatformPCGBreakSpansByTagsSettings final : public UGamePlatformPCGPointNodeSettings
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("GP_BreakSpansByTags"); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("GamePlatformPCG", "BreakSpansByTags", "GamePlatform | PCG | Break Spans By Tags"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Linear")
    TArray<FString> BlockingTags;

protected:
    virtual FPCGElementPtr CreateElement() const override;
};

class GAMEPLATFORMPCG_API FGamePlatformPCGBreakSpansByTagsElement final : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
    virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings*) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class GAMEPLATFORMPCG_API UGamePlatformPCGBuildRowsSettings final : public UGamePlatformPCGPointNodeSettings
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("GP_BuildRows"); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("GamePlatformPCG", "BuildRows", "GamePlatform | PCG | Build Rows"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
#endif

    /** M1点级规则：将已在地块内生成的候选点吸附到稳定垄距；多边形裁剪仍由TPL_CropField模板负责。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Parcel", meta=(ClampMin="1.0", PCG_Overridable))
    float RowSpacingCm = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Parcel", meta=(PCG_Overridable))
    float RowYawDegrees = 0.0f;

protected:
    virtual FPCGElementPtr CreateElement() const override;
};

class GAMEPLATFORMPCG_API FGamePlatformPCGBuildRowsElement final : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
    virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings*) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class GAMEPLATFORMPCG_API UGamePlatformPCGSelectSpanMeshByLengthSettings final : public UGamePlatformPCGPointNodeSettings
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("GP_SelectSpanMeshByLength"); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("GamePlatformPCG", "SelectSpanMeshByLength", "GamePlatform | PCG | Select Span Mesh By Length"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Linear")
    TArray<FGamePlatformPCGSpanMeshRule> Rules;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Linear")
    FName FallbackMeshSetId = NAME_None;

protected:
    virtual FPCGElementPtr CreateElement() const override;
};

class GAMEPLATFORMPCG_API FGamePlatformPCGSelectSpanMeshByLengthElement final : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
    virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings*) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
};

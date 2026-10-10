#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "Definitions/GamePlatformPCGBakeManifest.h"
#include "Validation/PCGPolicy.h"
#include "Schema/GamePlatformPCGSchema.h"

namespace
{
/** Profile（配置）中的可选Definition引用仍必须进入RequiredDefinitions，统一由GamePlatformData建立租约。 */
FGamePlatformResult ValidateOptionalDeclaredDependency(
    const UGamePlatformDefinitionBase& Owner,
    const FPrimaryAssetId& Id)
{
    if (!Id.IsValid())
    {
        return FGamePlatformResult::Success();
    }

    if (Id.PrimaryAssetType != UGamePlatformPrimaryDataAsset::DefinitionAssetType())
    {
        return FGamePlatformResult::Failure(
            TEXT("PCGInvalidTemplateDefinitionRef"),
            TEXT("PCG模板引用必须属于统一GamePlatformDefinition主资产类型。"));
    }

    return Owner.RequiredDefinitions.Contains(Id)
        ? FGamePlatformResult::Success()
        : FGamePlatformResult::Failure(
            TEXT("PCGUndeclaredTemplateDependency"),
            TEXT("PCG模板引用必须同步登记到RequiredDefinitions，统一交由GamePlatformData加载。"));
}
}

FGamePlatformResult UGamePlatformPCGProfileDefinition::ValidateDefinition() const
{
    const auto Base = Super::ValidateDefinition(); if (!Base.IsSuccess()) { return Base; }
    if (ExecutionPolicy == EGamePlatformPCGExecutionPolicy::RuntimeAuthoritative)
    { return FGamePlatformResult::Unsupported(TEXT("RuntimeAuthorityUnsupported"),TEXT("本期不支持运行时权威生成")); }
    if ((ExecutionPolicy != EGamePlatformPCGExecutionPolicy::RuntimeCosmetic && ExecutionPolicy != EGamePlatformPCGExecutionPolicy::EditorGeneratedStatic) ||
        (OutputUsage != EGamePlatformPCGOutputUsage::Cosmetic && OutputUsage != EGamePlatformPCGOutputUsage::StaticCollision))
    { return FGamePlatformResult::Failure(TEXT("InvalidPolicy"),TEXT("未声明的生成策略或输出用途")); }
    if (ExecutionPolicy == EGamePlatformPCGExecutionPolicy::RuntimeCosmetic && OutputUsage != EGamePlatformPCGOutputUsage::Cosmetic)
    { return FGamePlatformResult::Failure(TEXT("RuntimeCollisionForbidden"),TEXT("运行时只允许无玩法影响的装饰")); }
    const bool bLegacyProfile = TemplateId.IsNone();
    if (GraphReference.IsNull() || (bLegacyProfile && OutputMesh.IsNull()) || !RegionId.IsValid() || MinimumOutputs < 0 || MaximumOutputs <= 0)
    { return FGamePlatformResult::Failure(TEXT("MissingProfileInput"),TEXT("图、Legacy网格、区域或输出数量不合法")); }
    if (RequiredPCGSchemaMajor != FGamePlatformPCGSchema::CurrentVersion().Major)
    { return FGamePlatformResult::Failure(TEXT("PCGSchemaVersionMismatch"),TEXT("Profile要求的PCG Schema主版本与当前平台不一致")); }
    if (!TemplateId.IsNone() && TemplateVersion <= 0)
    { return FGamePlatformResult::Failure(TEXT("PCGTemplateVersionMissing"),TEXT("声明TemplateId时必须同时提供正数TemplateVersion")); }
    for (const FPrimaryAssetId& OptionalDefinition : {ExecPresetId, PriorityTableId, MeshSetDefinitionId})
    {
        const FGamePlatformResult Dependency = ValidateOptionalDeclaredDependency(*this, OptionalDefinition);
        if (!Dependency.IsSuccess()) { return Dependency; }
    }
    GamePlatformPCGPolicy::NumericProfile Numeric;
    Numeric.HalfExtentCm = {HalfExtentCm.X,HalfExtentCm.Y,HalfExtentCm.Z}; Numeric.SpacingCm = SpacingCm;
    Numeric.Density = Density; Numeric.Scale = UniformScale; Numeric.TimeoutSeconds = TimeoutSeconds;
    Numeric.MinOutputs = static_cast<uint32>(MinimumOutputs); Numeric.MaxOutputs = static_cast<uint32>(MaximumOutputs);
    const auto Error = GamePlatformPCGPolicy::Validate(Numeric);
    return Error.empty() ? FGamePlatformResult::Success() : FGamePlatformResult::Failure(FName(UTF8_TO_TCHAR(Error.c_str())),TEXT("PCG参数超出有限初始测试范围"));
}
FGamePlatformResult UGamePlatformPCGBakeManifest::ValidateDefinition() const
{
    const auto Base = Super::ValidateDefinition(); if (!Base.IsSuccess()) { return Base; }
    if (!WorldId.IsValid() || !RegionId.IsValid() || !ProfileId.IsValid() || ProfileRevision <= 0 || SourceFingerprint.IsEmpty() ||
        OutputFingerprint.IsEmpty() || SourceDependencies.IsEmpty() || InstanceCount < 0 || OwnedOutput.IsNull() || GeneratorVersion.IsEmpty())
    { return FGamePlatformResult::Failure(TEXT("IncompleteBakeManifest"),TEXT("来源或实际输出记录缺失；不得成为发布依据")); }
    if (PCGSchemaMajor != FGamePlatformPCGSchema::CurrentVersion().Major || GeneratedActorCount < 0)
    { return FGamePlatformResult::Failure(TEXT("InvalidPCGBakeContract"),TEXT("Bake Manifest的PCG Schema版本或生成Actor数量非法")); }
    if (!TemplateId.IsNone() && TemplateVersion <= 0)
    { return FGamePlatformResult::Failure(TEXT("InvalidPCGTemplateManifest"),TEXT("Bake Manifest声明TemplateId时必须记录正数TemplateVersion")); }
    return FGamePlatformResult::Success();
}

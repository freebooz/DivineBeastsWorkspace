#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "Definitions/GamePlatformPCGBakeManifest.h"
#include "Validation/PCGPolicy.h"

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
    if (GraphReference.IsNull() || OutputMesh.IsNull() || !RegionId.IsValid() || MinimumOutputs < 0 || MaximumOutputs <= 0)
    { return FGamePlatformResult::Failure(TEXT("MissingProfileInput"),TEXT("图、网格、区域或输出数量不合法")); }
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
    return FGamePlatformResult::Success();
}

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Validation/GamePlatformValidationTypes.h"
#include "GamePlatformStaticValidators.generated.h"

/** UGamePlatformDependencyValidator（三层依赖与循环依赖验证器）。 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformDependencyValidator : public UObject
{
    GENERATED_BODY()
public:
    void ValidateWorkspace(TArray<FGamePlatformValidationResult>& OutResults) const;
    bool WriteDependencyGraphJson(const FString& OutputPath, FString& OutError) const;
};

/** UGamePlatformClientLeakValidator（客户端泄漏验证器）；只对真实构建/Cook工件给结论。 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformClientLeakValidator : public UObject
{
    GENERATED_BODY()
public:
    FGamePlatformValidationResult ValidateArtifactRoot(const FString& ArtifactRoot) const;
};

/** UGamePlatformServerLeakValidator（服务器泄漏验证器）；只对真实构建/Cook工件给结论。 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformServerLeakValidator : public UObject
{
    GENERATED_BODY()
public:
    FGamePlatformValidationResult ValidateArtifactRoot(const FString& ArtifactRoot) const;
};

/** UGamePlatformRPCAndAuthorityValidator（RPC/Authority静态验证器）。 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformRPCAndAuthorityValidator : public UObject
{
    GENERATED_BODY()
public:
    void ValidateWorkspace(TArray<FGamePlatformValidationResult>& OutResults) const;
};

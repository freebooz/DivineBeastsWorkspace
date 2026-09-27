#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GamePlatformValidationCommandlet.generated.h"

/**
 * UGamePlatformValidationCommandlet（游戏平台自定义验证命令行工具）。
 * 只覆盖标准DataValidation无法覆盖的架构/Cook工件/Release门禁。
 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformValidationCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UGamePlatformValidationCommandlet();

    virtual int32 Main(const FString& Params) override;
};

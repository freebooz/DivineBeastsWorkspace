#pragma once

#include "Commandlets/Commandlet.h"
#include "GamePlatformPCGFoundationTemplatesCommandlet.generated.h"

/**
 * UGamePlatformPCGFoundationTemplatesCommandlet（PCG基础模板生成命令行工具）。
 * 仅Editor目标可用；显式创建/Game/Development下M0/M1模板，不覆盖已有资产，不执行世界生成。
 */
UCLASS()
class UGamePlatformPCGFoundationTemplatesCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    UGamePlatformPCGFoundationTemplatesCommandlet();
    virtual int32 Main(const FString& Params) override;
};

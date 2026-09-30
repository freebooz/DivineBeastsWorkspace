// 平台编辑器设置校验：读取项目设置CDO与已注册描述，收集Client/Server静态合同错误；不启动世界或读取用户存档。
// 同步结果及错误数组归调用方，失败不修改Provider/用户值；直接消费Core结果实现，须声明模块链接依赖。
#include "Validation/GamePlatformSettingsEditorValidator.h"

#include "Settings/GamePlatformSettingsProjectSettings.h"
#include "Types/GamePlatformResult.h"
#include "Validation/GamePlatformSettingsValidationLibrary.h"

bool FGamePlatformSettingsEditorValidator::ValidateAll(
    TArray<FString>& OutErrors)
{
    OutErrors.Reset();

    const UGamePlatformSettingsProjectSettings* Settings =
        GetDefault<UGamePlatformSettingsProjectSettings>();
    const int32 MaxProviders = Settings ? Settings->MaxProviders : 64;
    const int32 MaxDescriptors = Settings ? Settings->MaxDescriptors : 1024;

    for (const EGamePlatformSettingRuntimeScope RuntimeScope :
        { EGamePlatformSettingRuntimeScope::Client,
          EGamePlatformSettingRuntimeScope::Server })
    {
        const FGamePlatformResult Result =
            FGamePlatformSettingsValidationLibrary::
                ValidateRegisteredProviders(
                    RuntimeScope,
                    MaxProviders,
                    MaxDescriptors);
        if (!Result.IsSuccess())
        {
            OutErrors.Add(
                FString::Printf(
                    TEXT("%s端设置Provider验证失败：%s / %s"),
                    RuntimeScope == EGamePlatformSettingRuntimeScope::Client
                        ? TEXT("Client")
                        : TEXT("Server"),
                    *Result.Code.ToString(),
                    *Result.Message));
        }
    }

    return OutErrors.IsEmpty();
}

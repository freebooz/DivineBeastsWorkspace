#include "Commands/GamePlatformPCGFoundationTemplatesCommandlet.h"

#include "Authoring/GamePlatformPCGEditorLibrary.h"
#include "GamePlatformPCGLog.h"

UGamePlatformPCGFoundationTemplatesCommandlet::UGamePlatformPCGFoundationTemplatesCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 UGamePlatformPCGFoundationTemplatesCommandlet::Main(const FString& Params)
{
    // 当前命令无外部参数；保留签名以符合UCommandlet接口。
    (void)Params;
    FString Error;
    if (!UGamePlatformPCGEditorLibrary::CreateFoundationTemplateAssets(Error))
    {
        UE_LOG(LogGamePlatformPCG, Error, TEXT("Foundation模板生成失败：%s"), *Error);
        return 1;
    }

    UE_LOG(LogGamePlatformPCG, Display, TEXT("Foundation模板生成完成：M0/M1开发模板已保存到/Game/Development/Foundation/PCG/Templates。"));
    return 0;
}

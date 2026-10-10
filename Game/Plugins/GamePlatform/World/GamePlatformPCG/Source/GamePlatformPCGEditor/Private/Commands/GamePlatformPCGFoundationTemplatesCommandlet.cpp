#include "Commands/GamePlatformPCGFoundationTemplatesCommandlet.h"

#include "Authoring/GamePlatformPCGEditorLibrary.h"
#include "GamePlatformPCGLog.h"
#include "Misc/Parse.h"

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
    // 蓝图属于项目ContentPack，需显式参数指定已注册挂载点；平台插件不持有项目包名。
    FString Error;
    FString BlueprintRoot;
    if (FParse::Value(*Params, TEXT("BlueprintRoot="), BlueprintRoot))
    {
        if (!UGamePlatformPCGEditorLibrary::CreatePCGPlacementBlueprints(BlueprintRoot, Error))
        {
            UE_LOG(LogGamePlatformPCG, Error, TEXT("PCG项目放置器蓝图生成失败：%s"), *Error);
            return 1;
        }
        UE_LOG(LogGamePlatformPCG, Display, TEXT("PCG放置器真实蓝图已由UE编辑器生成；需要独立重开和人工检查。"));
        return 0;
    }
    if (!UGamePlatformPCGEditorLibrary::CreateFoundationAssets(Error))
    {
        UE_LOG(LogGamePlatformPCG, Error, TEXT("Foundation模板/子图生成失败：%s"), *Error);
        return 1;
    }

    UE_LOG(LogGamePlatformPCG, Display, TEXT("Foundation资产生成完成：M0/M1模板与公共子图已保存到/Game/Development/Foundation/PCG。"));
    return 0;
}

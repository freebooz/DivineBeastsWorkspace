// GamePlatformSurface核心资产Commandlet实现。
#include "Commands/GamePlatformSurfaceCoreAssetsCommandlet.h"

#include "Authoring/GamePlatformSurfaceEditorLibrary.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogGamePlatformSurfaceEditor, Log, All);

UGamePlatformSurfaceCoreAssetsCommandlet::UGamePlatformSurfaceCoreAssetsCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 UGamePlatformSurfaceCoreAssetsCommandlet::Main(const FString& Params)
{
    const bool bValidateOnly = FParse::Param(*Params, TEXT("ValidateOnly"));

    FString Error;
    if (!GamePlatformSurfaceEditor::EnsureCoreParameterCollection(bValidateOnly, Error))
    {
        UE_LOG(LogGamePlatformSurfaceEditor, Error, TEXT("Surface核心MPC处理失败：%s"), *Error);
        return 1;
    }

    UE_LOG(
        LogGamePlatformSurfaceEditor,
        Display,
        TEXT("Surface核心MPC%s成功。"),
        bValidateOnly ? TEXT("验证") : TEXT("创建/验证"));
    return 0;
}

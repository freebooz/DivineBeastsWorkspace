// 本文件属于GamePlatform平台层 GamePlatformVFX，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#include "Integration/Presentation/GamePlatformVFXPresentationBridgeSubsystem.h"
#include "Integration/Presentation/GamePlatformVFXPresentationProvider.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "Engine/LocalPlayer.h"

void UGamePlatformVFXPresentationBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UGamePlatformPresentationClientSubsystem>();

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        PresentationSubsystem = LocalPlayer->GetSubsystem<UGamePlatformPresentationClientSubsystem>();
    }

    if (IsValid(PresentationSubsystem))
    {
        ProviderRegistrationId = PresentationSubsystem->RegisterProvider(
            TEXT("VFX"),
            100,
            FGamePlatformPresentationProviderHandler::CreateUObject(
                this,
                &UGamePlatformVFXPresentationBridgeSubsystem::HandlePresentationRequest), UGamePlatformVFXDefinition::StaticClass());
    }
}

void UGamePlatformVFXPresentationBridgeSubsystem::Deinitialize()
{
    if (IsValid(PresentationSubsystem) && ProviderRegistrationId.IsValid())
    {
        PresentationSubsystem->UnregisterProvider(ProviderRegistrationId);
    }
    ProviderRegistrationId.Invalidate();
    PresentationSubsystem = nullptr;
    Super::Deinitialize();
}

bool UGamePlatformVFXPresentationBridgeSubsystem::HandlePresentationRequest(
    const FGamePlatformPresentationRequest& Request)
{
    FGamePlatformVFXPresentationProvider Provider(GetWorld());
    return Provider.Handle(Request);
}

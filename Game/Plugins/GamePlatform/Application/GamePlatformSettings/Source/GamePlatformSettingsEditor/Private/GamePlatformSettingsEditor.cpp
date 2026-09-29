#include "Modules/ModuleManager.h"

#include "GamePlatformSettingsLog.h"
#include "Validation/GamePlatformSettingsEditorValidator.h"

class FGamePlatformSettingsEditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        TArray<FString> Errors;
        if (!FGamePlatformSettingsEditorValidator::ValidateAll(Errors))
        {
            for (const FString& Error : Errors)
            {
                UE_LOG(
                    LogGamePlatformSettings,
                    Error,
                    TEXT("%s"),
                    *Error);
            }
        }
    }
};

IMPLEMENT_MODULE(
    FGamePlatformSettingsEditorModule,
    GamePlatformSettingsEditor)

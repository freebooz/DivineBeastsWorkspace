#include "Settings/GamePlatformValidationSettings.h"

UGamePlatformValidationSettings::UGamePlatformValidationSettings()
{
    CategoryName = TEXT("Game Platform");
    SectionName = TEXT("Validation");

    AssetClassPrefixes =
    {
        { TEXT("Texture"), TEXT("T_") },
        { TEXT("MaterialInstance"), TEXT("MI_") },
        { TEXT("Material"), TEXT("M_") },
        { TEXT("StaticMesh"), TEXT("SM_") },
        { TEXT("SkeletalMesh"), TEXT("SK_") },
        { TEXT("NiagaraSystem"), TEXT("NS_") },
        { TEXT("WidgetBlueprint"), TEXT("WBP_") },
        { TEXT("Blueprint"), TEXT("BP_") },
        { TEXT("Sound"), TEXT("S_") }
    };

    RemovedGameplayTagPrefixes =
    {
        TEXT("FiveCamp."),
        TEXT("Faction."),
        TEXT("Element."),
        TEXT("KingSeal.")
    };

    ForbiddenServerDependencyPathTokens =
    {
        TEXT("/Client/"),
        TEXT("/Presentation/"),
        TEXT("/UI/"),
        TEXT("/VFX/"),
        TEXT("/SFX/"),
        TEXT("/Cinematic/"),
        TEXT("/CommerceUI/"),
        TEXT("/GamePlatformDebug/")
    };
}

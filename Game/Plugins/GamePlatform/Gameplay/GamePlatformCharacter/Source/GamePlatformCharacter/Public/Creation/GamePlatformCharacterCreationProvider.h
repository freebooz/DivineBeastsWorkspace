#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

/** FGamePlatformCharacterCreationHeroDescriptor（平台角色创建英雄描述）。 */
struct FGamePlatformCharacterCreationHeroDescriptor
{
    FName HeroDefinitionId = NAME_None;
    FName DisplayNameKey = NAME_None;
    FString ContentRevision;
};

/**
 * IGamePlatformCharacterCreationProvider（平台角色创建提供者）。
 * 仅提供本地Catalog/外观Schema与基础校验，不执行后端创建、最终资格或持久化。
 */
class IGamePlatformCharacterCreationProvider
    : public IModularFeature
{
public:
    virtual ~IGamePlatformCharacterCreationProvider() = default;

    static FName GetModularFeatureName()
    {
        static const FName Name(TEXT("GamePlatform.CharacterCreationProvider"));
        return Name;
    }

    virtual int32 GetCatalogRevision() const = 0;

    virtual void GetCreateableHeroes(
        TArray<FGamePlatformCharacterCreationHeroDescriptor>& OutHeroes) const = 0;

    virtual bool ValidateCreationDraft(
        FName HeroDefinitionId,
        const TMap<FString, FString>& AppearanceSelection,
        FString& OutError) const = 0;
};

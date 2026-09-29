#pragma once

#include "Screens/DivineBeastsUIScreen.h"
#include "DivineBeastsCharacterCreateScreen.generated.h"

class UDivineBeastsCharacterCreateViewModel;

/** UDivineBeastsCharacterCreateScreen（持久角色创建页面基类）。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsCharacterCreateScreen
    : public UDivineBeastsUIScreen
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterCreate")
    FName GetCharacterCreateScreenId() const { return TEXT("UI.Screen.CharacterCreate"); }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterCreate")
    UDivineBeastsCharacterCreateViewModel* GetCharacterCreateViewModel() const;
};

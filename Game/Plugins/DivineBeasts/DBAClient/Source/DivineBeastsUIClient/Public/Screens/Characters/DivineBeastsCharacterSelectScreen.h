#pragma once

#include "Screens/DivineBeastsUIScreen.h"
#include "DivineBeastsCharacterSelectScreen.generated.h"

class UDivineBeastsCharacterSelectViewModel;

/** UDivineBeastsCharacterSelectScreen（持久角色选择页面基类）。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsCharacterSelectScreen
    : public UDivineBeastsUIScreen
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterSelect")
    FName GetCharacterSelectScreenId() const { return TEXT("UI.Screen.CharacterSelect"); }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterSelect")
    UDivineBeastsCharacterSelectViewModel* GetCharacterSelectViewModel() const;
};

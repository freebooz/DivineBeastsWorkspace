#include "Screens/System/DivineBeastsSystemScreenBase.h"

#include "Engine/LocalPlayer.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"

FGamePlatformUIAccessibilityPreferences
UDivineBeastsSystemScreenBase::GetAccessibilityPreferences() const
{
    ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
    const UGamePlatformUIManagerSubsystem* Manager =
        LocalPlayer ? LocalPlayer->GetSubsystem<UGamePlatformUIManagerSubsystem>() : nullptr;
    return Manager ? Manager->GetAccessibilityPreferences() :
        FGamePlatformUIAccessibilityPreferences();
}

bool UDivineBeastsSystemScreenBase::RequestAccessibilityPreferences(
    FGamePlatformUIAccessibilityPreferences Preferences)
{
    ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
    UGamePlatformUIManagerSubsystem* Manager =
        LocalPlayer ? LocalPlayer->GetSubsystem<UGamePlatformUIManagerSubsystem>() : nullptr;
    if (!IsActivated() || !IsValid(Manager))
    {
        return false;
    }
    // 校验和广播由平台统一完成，本方法不引入第二份设置状态缓存。
    Manager->SetAccessibilityPreferences(Preferences);
    return true;
}

#pragma once

#include "Contracts/DivineBeastsUIDomainTypes.h"
#include "GamePlatformUITypes.h"
#include "Screens/DivineBeastsMenuScreen.h"
#include "DivineBeastsSystemScreenBase.generated.h"

/**
 * UDivineBeastsSystemScreenBase（系统设置页面基类）。
 * 只复用GamePlatformUI已经存在的可访问性偏好；设备图形、声音及输入等
 * 持久设置仍归GamePlatformSettings/Input/SFX，项目UI不得另建持久化真源。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsSystemScreenBase
    : public UDivineBeastsMenuScreen
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Domain")
    EDivineBeastsUIDomain GetBusinessDomain() const { return EDivineBeastsUIDomain::System; }
    /** 从LocalPlayer的现有平台UI管理器读取当前辅助功能选项。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|System")
    FGamePlatformUIAccessibilityPreferences GetAccessibilityPreferences() const;
    /** 经平台管理器校验后调整UI偏好，不写磁盘，不更改玩法和设备设置。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|System")
    bool RequestAccessibilityPreferences(FGamePlatformUIAccessibilityPreferences Preferences);
};

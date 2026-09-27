#pragma once

#include "Screens/DivineBeastsUIScreen.h"
#include "DivineBeastsLoginScreen.generated.h"

class UDivineBeastsLoginViewModel;

/**
 * UDivineBeastsLoginScreen（神兽联盟登录页面 C++ 基类）。
 *
 * 职责：
 * - 作为 WBP_DBA_UI_Login 等登录视觉蓝图的唯一项目父类。
 * - 复用平台 Screen 的焦点、输入模式、PC/Mobile 自适应和事件驱动 ViewModel 生命周期。
 * - 不直接调用认证服务；登录意图必须经 UDivineBeastsLoginViewModel 提交。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsLoginScreen
    : public UDivineBeastsUIScreen
{
    GENERATED_BODY()

public:
    /** 返回稳定页面标识，与项目 UI Catalog 保持一致。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Login")
    FName GetLoginScreenId() const
    {
        return TEXT("UI.Screen.Login");
    }

    /** 返回类型安全的登录 ViewModel；类型不匹配时返回 nullptr。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Login")
    UDivineBeastsLoginViewModel* GetLoginViewModel() const;
};

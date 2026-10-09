#pragma once

#include "Screens/DivineBeastsUIScreen.h"
#include "DivineBeastsAccountScreenBase.generated.h"
struct FDivineBeastsUIViewState;

/** 账号页面公共基类：只消费已确认的认证状态，不发起直接HTTP、不保存密码。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsAccountScreenBase : public UDivineBeastsUIScreen
{
    GENERATED_BODY()
public:
    UDivineBeastsAccountScreenBase() { UIDomain = EDivineBeastsUIDomain::Account; }

    /** 后端已确认登录时才返回true。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Account")
    bool IsAccountAuthenticated() const;
    /** 缺少状态源、正在提交或维护时禁止发起新的交互意图。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Account")
    bool IsAccountActionBusy() const;
    /** 统一显示已脱敏的流程错误文本。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Account")
    FText GetAccountErrorText() const;
private:
    const FDivineBeastsUIViewState* GetAccountState() const;
};

#pragma once

#include "ViewModels/DivineBeastsUIViewModel.h"
#include "DivineBeastsLoginViewModel.generated.h"

/**
 * UDivineBeastsLoginViewModel（神兽联盟登录视图模型）。
 *
 * 复用项目通用 UI Command（界面命令）与只读状态投影，并为登录页面提供轻量可用性判断。
 * 密码仍只通过 Login 命令瞬时传递，不进入 ViewState、日志或遥测。
 */
UCLASS(BlueprintType, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsLoginViewModel
    : public UDivineBeastsUIViewModel
{
    GENERATED_BODY()

public:
    /**
     * 判断当前页面是否允许提交账号凭据。
     * 这里只判断 UI 可用性；账号是否合法仍由后端认证权威决定。
     */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Login")
    bool CanSubmitCredentials() const
    {
        // 使用 const 引用读取统一视图状态，避免为一次按钮可用性判断复制角色/竞技容器。
        const FDivineBeastsUIViewState& State = GetStateRef();
        return IsPageActive() && !State.bBusy && !State.bMaintenance;
    }
};

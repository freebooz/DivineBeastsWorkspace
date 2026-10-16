#pragma once

// SSLM客户端开发登录配置的私有解析边界：仅消费进程环境提供的十账号JSON。
// 不持久化、不记录口令，也不替代真实认证；调用方只保留账号名，点击时瞬时读取一份口令。
#include "CoreMinimal.h"

namespace DivineBeasts::UI::DevelopmentLogin
{
    /** 验证恰好十个唯一账号及非空口令，成功仅返回可显示名称；失败清空输出。 */
    bool ReadAccountNames(const FString& Json, TArray<FString>& OutAccountNames);

    /** 点击时重新验证全配置及选中名称，防止旧按钮索引登录另一账号；失败清空两项输出。 */
    bool ReadCredential(const FString& Json, int32 Index, const FString& ExpectedAccountName,
        FString& OutAccountName, FString& OutPassword);
}

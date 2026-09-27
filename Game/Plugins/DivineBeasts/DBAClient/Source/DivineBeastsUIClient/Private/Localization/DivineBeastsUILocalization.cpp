#include "Localization/DivineBeastsUILocalization.h"

#define LOCTEXT_NAMESPACE "DivineBeastsUI"

FText FDivineBeastsUILocalization::ErrorCodeToText(FName ErrorCode)
{
    if (ErrorCode.IsNone()) { return FText::GetEmpty(); }
    if (ErrorCode == TEXT("InvalidCredentials")) { return LOCTEXT("InvalidCredentials", "账号或密码错误。"); }
    if (ErrorCode == TEXT("AccountLocked")) { return LOCTEXT("AccountLocked", "账号暂时不可用，请稍后重试。"); }
    if (ErrorCode == TEXT("Maintenance")) { return LOCTEXT("Maintenance", "服务正在维护，请稍后再试。"); }
    if (ErrorCode == TEXT("NetworkUnavailable")) { return LOCTEXT("NetworkUnavailable", "网络不可用，请检查网络后重试。"); }
    if (ErrorCode == TEXT("AuthExpired")) { return LOCTEXT("AuthExpired", "登录状态已失效，请重新登录。"); }
    if (ErrorCode == TEXT("NoServerCapacity")) { return LOCTEXT("NoServerCapacity", "当前服务器繁忙，请稍后重试。"); }
    if (ErrorCode == TEXT("ReconnectExhausted")) { return LOCTEXT("ReconnectExhausted", "重连失败，请返回并重新进入。"); }
    if (ErrorCode == TEXT("HeroCatalogUnavailable")) { return LOCTEXT("HeroCatalogUnavailable", "角色目录暂不可用。"); }
    if (ErrorCode == TEXT("InvalidAppearance")) { return LOCTEXT("InvalidAppearance", "当前外观选择不可用。"); }
    if (ErrorCode == TEXT("NotConfigured")) { return LOCTEXT("NotConfigured", "该功能当前尚未开放。"); }
    return LOCTEXT("GenericError", "操作未完成，请稍后重试。");
}

#undef LOCTEXT_NAMESPACE

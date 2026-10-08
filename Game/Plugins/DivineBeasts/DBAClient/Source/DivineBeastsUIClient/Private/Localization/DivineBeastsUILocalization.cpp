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

#define LOCTEXT_NAMESPACE "DivineBeastsHeroNames"
FText FDivineBeastsUILocalization::HeroNameToText(FName HeroDefinitionId)
{
    // 显示文案属于项目客户端；这里不决定创建资格，不把生肖规则带入平台。
    if (HeroDefinitionId == TEXT("Hero.Zodiac.Rat")) { return LOCTEXT("Rat", "子鼠 · 影牙"); }
    if (HeroDefinitionId == TEXT("Hero.Zodiac.Ox")) { return LOCTEXT("Ox", "丑牛 · 玄角"); }
    if (HeroDefinitionId == TEXT("Hero.Zodiac.Tiger")) { return LOCTEXT("Tiger", "寅虎 · 白君"); }
    if (HeroDefinitionId == TEXT("Hero.Zodiac.Rabbit")) { return LOCTEXT("Rabbit", "卯兔 · 玉灵"); }
    if (HeroDefinitionId == TEXT("Hero.Zodiac.Dragon")) { return LOCTEXT("Dragon", "辰龙 · 苍龙"); }
    if (HeroDefinitionId == TEXT("Hero.Zodiac.Snake")) { return LOCTEXT("Snake", "巳蛇 · 幽鳞"); }
    if (HeroDefinitionId == TEXT("Hero.Zodiac.Horse")) { return LOCTEXT("Horse", "午马 · 雷蹄"); }
    if (HeroDefinitionId == TEXT("Hero.Zodiac.Goat")) { return LOCTEXT("Goat", "未羊 · 玉角"); }
    if (HeroDefinitionId == TEXT("Hero.Zodiac.Monkey")) { return LOCTEXT("Monkey", "申猴 · 灵猴"); }
    if (HeroDefinitionId == TEXT("Hero.Zodiac.Rooster")) { return LOCTEXT("Rooster", "酉鸡 · 金鸣"); }
    if (HeroDefinitionId == TEXT("Hero.Zodiac.Dog")) { return LOCTEXT("Dog", "戌狗 · 天犬"); }
    if (HeroDefinitionId == TEXT("Hero.Zodiac.Boar")) { return LOCTEXT("Boar", "亥猪 · 玄鬃"); }
    return LOCTEXT("UnknownHero", "未知英雄");
}
#undef LOCTEXT_NAMESPACE

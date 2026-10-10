// 平台主题解析：资格过滤→精确语义→scope/等值条件具体度/优先级；同级冲突拒绝。
#include "Definitions/GamePlatformUIThemeDefinition.h"

namespace
{
bool IsStyleKey(FName Key)
{
    const FString Text = Key.ToString();
    return !Key.IsNone() && Text.StartsWith(TEXT("UI.Style.")) && Text.Len() > 9 &&
        !Text.EndsWith(TEXT(".")) && !Text.Contains(TEXT("..")) && !Text.Contains(TEXT("/")) &&
        !Text.Contains(TEXT(" ")) && Text.Len() <= 160;
}

template<typename EntryType>
bool ValidateEntries(const TArray<EntryType>& Entries, FString& Error)
{
    for (int32 I = 0; I < Entries.Num(); ++I)
    {
        const auto& Entry = Entries[I];
        const auto& R = Entry.Rule;
        if (!IsStyleKey(R.StyleId) || Entry.StyleClass.IsNull() ||
            R.QualityTier < -1 || R.QualityTier > 4 || FMath::Abs(int64(R.Priority)) > 1000 ||
            uint8(R.Scope) > uint8(EGamePlatformUIStyleScope::ContentPack))
        {
            Error = FString::Printf(TEXT("无效主题规则或缺少样式软类：%s"), *R.StyleId.ToString());
            return false;
        }
        for (int32 J = 0; J < I; ++J)
        {
            const auto& P = Entries[J].Rule;
            if (R.StyleId == P.StyleId && R.Scope == P.Scope && R.PlatformId == P.PlatformId &&
                R.SkinId == P.SkinId && R.QualityTier == P.QualityTier && R.Priority == P.Priority)
            {
                Error = FString::Printf(TEXT("主题存在完全重复的选择条件：%s"), *R.StyleId.ToString());
                return false;
            }
        }
    }
    return true;
}

template<typename EntryType>
int32 Resolve(const TArray<EntryType>& Entries, FName Key,
    const FGamePlatformUIThemeContext& Context, bool bFallback, FText& Error)
{
    Error = FText::GetEmpty();
    if (!IsStyleKey(Key) || !Context.IsValid())
    {
        Error = FText::FromString(TEXT("主题语义或上下文非法。"));
        return INDEX_NONE;
    }
    FString Semantic = Key.ToString();
    while (Semantic.StartsWith(TEXT("UI.Style.")))
    {
        const FName CandidateKey(*Semantic);
        int32 Best = INDEX_NONE;
        bool bAmbiguous = false;
        for (int32 I = 0; I < Entries.Num(); ++I)
        {
            const auto& Rule = Entries[I].Rule;
            if (Rule.StyleId != CandidateKey || !Rule.Matches(Context)) continue;
            if (Best == INDEX_NONE) { Best = I; bAmbiguous = false; continue; }
            const auto& Current = Entries[Best].Rule;
            const int32 ScopeDiff = int32(Rule.Scope) - int32(Current.Scope);
            const int32 SpecificDiff = Rule.Specificity() - Current.Specificity();
            const int64 PriorityDiff = int64(Rule.Priority) - int64(Current.Priority);
            const bool bBetter = ScopeDiff > 0 || (ScopeDiff == 0 &&
                (SpecificDiff > 0 || (SpecificDiff == 0 && PriorityDiff > 0)));
            if (bBetter) { Best = I; bAmbiguous = false; }
            else if (ScopeDiff == 0 && SpecificDiff == 0 && PriorityDiff == 0) bAmbiguous = true;
        }
        if (Best != INDEX_NONE)
        {
            if (!bAmbiguous) return Best;
            Error = FText::FromString(FString::Printf(TEXT("主题规则歧义，不能按数组顺序选择：%s"), *Semantic));
            return INDEX_NONE;
        }
        if (!bFallback) break;
        int32 Dot = INDEX_NONE;
        if (!Semantic.FindLastChar(TEXT('.'), Dot) || Dot <= 8) break;
        Semantic.LeftInline(Dot);
    }
    Error = FText::FromString(FString::Printf(TEXT("当前上下文缺少主题样式：%s"), *Key.ToString()));
    return INDEX_NONE;
}

template<typename EntryType, typename StyleType>
bool ValidateLoadedEntries(const TArray<EntryType>& Entries, FText& Error)
{
    for (const auto& Entry : Entries)
    {
        const UClass* Class = Entry.StyleClass.Get();
        if (!Class || !Class->IsChildOf(StyleType::StaticClass()) ||
            Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        {
            Error = FText::FromString(FString::Printf(TEXT("主题样式未加载或反射类型不合法：%s"),
                *Entry.Rule.StyleId.ToString()));
            return false;
        }
    }
    return true;
}
}

FGamePlatformResult UGamePlatformUIThemeDefinition::ValidateDefinition() const
{
    const auto Parent = Super::ValidateDefinition();
    if (!Parent.IsSuccess()) return Parent;
    const int64 Count = int64(Buttons.Num()) + Texts.Num() + Borders.Num();
    if (Count < 1 || Count > 128 || RequiredStyles.Num() > 128)
        return FGamePlatformResult::Failure(TEXT("ThemeSizeInvalid"), TEXT("主题必须含1至128个样式，必需语义最多128项。"));
    FString Error;
    if (!ValidateEntries(Buttons, Error) || !ValidateEntries(Texts, Error) || !ValidateEntries(Borders, Error))
        return FGamePlatformResult::Failure(TEXT("ThemeRuleInvalid"), Error);
    TSet<FString> RequiredKeys;
    for (const auto& Required : RequiredStyles)
    {
        const FString Identity = FString::Printf(TEXT("%d:%s"), int32(Required.Kind), *Required.StyleId.ToString());
        if (!IsStyleKey(Required.StyleId) || uint8(Required.Kind) > uint8(EGamePlatformUIStyleKind::Border) ||
            RequiredKeys.Contains(Identity))
            return FGamePlatformResult::Failure(TEXT("ThemeRequirementInvalid"), TEXT("必需样式的类型、身份或唯一性非法。"));
        RequiredKeys.Add(Identity);
    }
    return FGamePlatformResult::Success();
}

int32 UGamePlatformUIThemeDefinition::ResolveButtonStyle(FName Key, const FGamePlatformUIThemeContext& Context, bool bFallback, FText& Error) const
{ return Resolve(Buttons, Key, Context, bFallback, Error); }
int32 UGamePlatformUIThemeDefinition::ResolveTextStyle(FName Key, const FGamePlatformUIThemeContext& Context, bool bFallback, FText& Error) const
{ return Resolve(Texts, Key, Context, bFallback, Error); }
int32 UGamePlatformUIThemeDefinition::ResolveBorderStyle(FName Key, const FGamePlatformUIThemeContext& Context, bool bFallback, FText& Error) const
{ return Resolve(Borders, Key, Context, bFallback, Error); }

UClass* UGamePlatformUIThemeDefinition::ResolveLoadedStyle(EGamePlatformUIStyleKind Kind, FName Key,
    const FGamePlatformUIThemeContext& Context, bool bFallback, FText& Error) const
{
    int32 Index = INDEX_NONE;
    switch (Kind)
    {
    case EGamePlatformUIStyleKind::Button:
        Index = ResolveButtonStyle(Key, Context, bFallback, Error);
        return Buttons.IsValidIndex(Index) ? Buttons[Index].StyleClass.Get() : nullptr;
    case EGamePlatformUIStyleKind::Text:
        Index = ResolveTextStyle(Key, Context, bFallback, Error);
        return Texts.IsValidIndex(Index) ? Texts[Index].StyleClass.Get() : nullptr;
    case EGamePlatformUIStyleKind::Border:
        Index = ResolveBorderStyle(Key, Context, bFallback, Error);
        return Borders.IsValidIndex(Index) ? Borders[Index].StyleClass.Get() : nullptr;
    default: Error = FText::FromString(TEXT("不支持的主题样式类型。")); return nullptr;
    }
}

bool UGamePlatformUIThemeDefinition::ValidateLoadedStyles(const FGamePlatformUIThemeContext& Context, FText& Error) const
{
    if (!ValidateDefinition().IsSuccess() || !Context.IsValid())
    {
        Error = FText::FromString(TEXT("主题定义或当前上下文校验失败。"));
        return false;
    }
    if (!ValidateLoadedEntries<FGamePlatformUIButtonThemeEntry, UCommonButtonStyle>(Buttons, Error) ||
        !ValidateLoadedEntries<FGamePlatformUITextThemeEntry, UCommonTextStyle>(Texts, Error) ||
        !ValidateLoadedEntries<FGamePlatformUIBorderThemeEntry, UCommonBorderStyle>(Borders, Error)) return false;
    const auto CheckAmbiguities = [&Context, &Error](const auto& Entries)
    {
        for (const auto& Entry : Entries)
        {
            if (!Entry.Rule.Matches(Context)) continue;
            if (Resolve(Entries, Entry.Rule.StyleId, Context, false, Error) == INDEX_NONE) return false;
        }
        return true;
    };
    if (!CheckAmbiguities(Buttons) || !CheckAmbiguities(Texts) || !CheckAmbiguities(Borders)) return false;
    for (const auto& Required : RequiredStyles)
        if (!ResolveLoadedStyle(Required.Kind, Required.StyleId, Context, false, Error)) return false;
    Error = FText::GetEmpty();
    return true;
}

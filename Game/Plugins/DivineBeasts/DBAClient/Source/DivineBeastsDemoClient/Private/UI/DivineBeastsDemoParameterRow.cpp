// SSLM类型化开发参数编辑；游戏线程，不写玩家设置/存档/权威数值，解析失败明确保留诊断。
#include "UI/DivineBeastsDemoParameterRow.h"
#include "Components/EditableTextBox.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "String/LexFromString.h"
#include "Containers/UnrealString.h"
#include "Catalog/DivineBeastsDemoCatalogValidation.h"

namespace
{
/** 先完整消费十进制语法，避免引擎宽松转换把50abc或1.5误当整数。实数允许指数，不允许空白/NaN/Inf。 */
bool IsDecimal(const FString& Text, bool bInteger)
{
    int32 Index = 0;
    if (Text.IsEmpty()) return false;
    if (Text[Index] == TEXT('-') || Text[Index] == TEXT('+')) ++Index;
    const auto Digits = [&Text, &Index]()
    { const int32 Start = Index; while (Index < Text.Len() && Text[Index] >= TEXT('0') && Text[Index] <= TEXT('9')) ++Index; return Index > Start; };
    const bool bWhole = Digits(); bool bFraction = false;
    if (!bInteger && Index < Text.Len() && Text[Index] == TEXT('.')) { ++Index; bFraction = Digits(); }
    if (!bWhole && !bFraction) return false;
    if (!bInteger && Index < Text.Len() && (Text[Index] == TEXT('e') || Text[Index] == TEXT('E')))
    { ++Index; if (Index < Text.Len() && (Text[Index] == TEXT('+') || Text[Index] == TEXT('-'))) ++Index; if (!Digits()) return false; }
    return Index == Text.Len();
}
}

bool UDivineBeastsDemoParameterItem::SetTextValue(const FString& Text)
{
    bValidInput = false;
    if (!Definition.bWritable || Definition.ApplyWhen != TEXT("execute") || Text.IsEmpty() || Text != Text.TrimStartAndEnd()) return false;
    auto Candidate = Value;
    switch (Definition.DefaultValue.Type)
    {
    case EDivineBeastsDemoParameterType::Integer:
        if (!IsDecimal(Text, true) || Text.Len() > 17 || !LexTryParseString(Candidate.IntegerValue, *Text) || Candidate.IntegerValue < -9007199254740991LL || Candidate.IntegerValue > 9007199254740991LL ||
            static_cast<double>(Candidate.IntegerValue) < Definition.Minimum || static_cast<double>(Candidate.IntegerValue) > Definition.Maximum) return false;
        break;
    case EDivineBeastsDemoParameterType::Number:
        if (!IsDecimal(Text, false) || Text.Len() > 128 || !LexTryParseString(Candidate.NumberValue, *Text) || !FMath::IsFinite(Candidate.NumberValue) || Candidate.NumberValue < Definition.Minimum || Candidate.NumberValue > Definition.Maximum) return false;
        break;
    case EDivineBeastsDemoParameterType::Identifier:
    {
        // 构造FName之前限制长度与ASCII格式，避免非法编辑文本触发引擎空名/超长名断言。
        if (!DivineBeasts::Demo::IsStableIdentityText(Text)) return false;
        Candidate.IdentifierValue = FName(*Text); break;
    }
    case EDivineBeastsDemoParameterType::AssetPath:
        if (!DivineBeasts::Demo::IsStableAssetPathText(Text)) return false;
        Candidate.AssetPathValue = FSoftObjectPath(Text); break;
    default: return false;
    }
    Value = Candidate; bValidInput = true; return true;
}
bool UDivineBeastsDemoParameterItem::SetBooleanValue(bool bValue)
{
    bValidInput = Definition.bWritable && Definition.ApplyWhen == TEXT("execute") && Value.Type == EDivineBeastsDemoParameterType::Boolean;
    if (bValidInput) Value.bBooleanValue = bValue;
    return bValidInput;
}
void UDivineBeastsDemoParameterRow::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (ParameterValueInput) ParameterValueInput->OnTextChanged.AddUniqueDynamic(this, &ThisClass::OnTextChanged);
    if (ParameterBooleanInput) ParameterBooleanInput->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::OnBooleanChanged);
}
void UDivineBeastsDemoParameterRow::NativeOnListItemObjectSet(UObject* Object)
{
    IUserObjectListEntry::NativeOnListItemObjectSet(Object); Item = Cast<UDivineBeastsDemoParameterItem>(Object);
    TGuardValue<bool> Guard(bSynchronizing, true);
    if (!Item.IsValid()) return;
    const auto& Definition = Item->Definition; const auto& Value = Item->Value;
    const bool bBoolean = Value.Type == EDivineBeastsDemoParameterType::Boolean;
    const bool bWritable = Definition.bWritable && Definition.ApplyWhen == TEXT("execute");
    if (ParameterLabelText) ParameterLabelText->SetText(FText::FromString(Definition.ParameterId.ToString() + TEXT("（") + Definition.Unit + TEXT("）")));
    if (ParameterErrorText) ParameterErrorText->SetText(FText::GetEmpty());
    if (ParameterBooleanInput) { ParameterBooleanInput->SetVisibility(bBoolean ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); ParameterBooleanInput->SetIsEnabled(bWritable); ParameterBooleanInput->SetIsChecked(Value.bBooleanValue); }
    if (ParameterValueInput)
    {
        ParameterValueInput->SetVisibility(bBoolean ? ESlateVisibility::Collapsed : ESlateVisibility::Visible); ParameterValueInput->SetIsEnabled(bWritable);
        const FString Text = Value.Type == EDivineBeastsDemoParameterType::AssetPath ? Value.AssetPathValue.ToString() :
            Value.Type == EDivineBeastsDemoParameterType::Identifier ? Value.IdentifierValue.ToString() :
            (Value.Type == EDivineBeastsDemoParameterType::Integer ? LexToString(Value.IntegerValue) : LexToString(Value.NumberValue));
        ParameterValueInput->SetText(FText::FromString(Text));
    }
}
void UDivineBeastsDemoParameterRow::OnTextChanged(const FText& Text)
{
    if (bSynchronizing || !Item.IsValid()) return;
    const bool bValid = Item->SetTextValue(Text.ToString());
    if (ParameterErrorText) ParameterErrorText->SetText(bValid ? FText::GetEmpty() : FText::FromString(TEXT("类型、范围或标识不合法，请修正后提交。")));
}
void UDivineBeastsDemoParameterRow::OnBooleanChanged(bool bChecked)
{ if (!bSynchronizing && Item.IsValid()) Item->SetBooleanValue(bChecked); }

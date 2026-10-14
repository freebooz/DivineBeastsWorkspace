// 公共列表行只改Monolith已有命名控件；回收时清空上个账号文字，选择只影响装饰，不修改业务状态。
#include "Widgets/Social/DivineBeastsPartyListEntry.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"

void UDivineBeastsPartyListEntry::NativeOnListItemObjectSet(UObject* Object)
{
    IUserObjectListEntry::NativeOnListItemObjectSet(Object);
    const auto* Item=Cast<UDivineBeastsPartyListItem>(Object);
    if (auto* Text=Cast<UTextBlock>(GetWidgetFromName(TEXT("RowTitle")))) Text->SetText(Item?Item->Title:FText());
    if (auto* Text=Cast<UTextBlock>(GetWidgetFromName(TEXT("RowDetail")))) Text->SetText(Item?Item->Detail:FText());
}
void UDivineBeastsPartyListEntry::NativeOnEntryReleased()
{
    if (auto* Text=Cast<UTextBlock>(GetWidgetFromName(TEXT("RowTitle")))) Text->SetText(FText());
    if (auto* Text=Cast<UTextBlock>(GetWidgetFromName(TEXT("RowDetail")))) Text->SetText(FText());
    IUserListEntry::NativeOnEntryReleased();
}
void UDivineBeastsPartyListEntry::NativeOnItemSelectionChanged(bool bSelected)
{
    IUserListEntry::NativeOnItemSelectionChanged(bSelected);
    if (auto* Border=Cast<UBorder>(GetWidgetFromName(TEXT("SelectionBorder"))))
        Border->SetBrushColor(bSelected?FLinearColor(0.12f,0.38f,0.25f,0.9f):FLinearColor(0.02f,0.07f,0.055f,0.8f));
}

// SSLM虚拟化列表条目只读绑定；实际控件、焦点和样式由Monolith资产提供。
#include "UI/DivineBeastsDemoMenuItem.h"
#include "Components/TextBlock.h"
void UDivineBeastsDemoMenuEntryWidget::NativeOnListItemObjectSet(UObject* Object)
{
    IUserObjectListEntry::NativeOnListItemObjectSet(Object);
    const auto* Item = Cast<UDivineBeastsDemoMenuItem>(Object);
    if (LabelText) LabelText->SetText(Item ? Item->Label : FText::GetEmpty());
    if (StatusText) StatusText->SetText(Item ? Item->Status : FText::GetEmpty());
}

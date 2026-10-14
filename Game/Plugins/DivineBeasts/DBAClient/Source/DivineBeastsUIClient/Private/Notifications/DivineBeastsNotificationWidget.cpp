// SSLM公共通知呈现适配；项目客户端使用平台通知生命周期，资产归DBAUIPack_Core，文本不包含业务权威。
#include "Notifications/DivineBeastsNotificationWidget.h"
#include "Components/TextBlock.h"

void UDivineBeastsNotificationWidget::ApplyNotificationRequest(const FGamePlatformUINotificationRequest& InRequest)
{
    Super::ApplyNotificationRequest(InRequest);
    // 先完成平台替换通知；读取最终请求而非外部可重入传入引用，避免显示被替换的旧正文。
    const auto Request = GetNotificationRequest();
    if (auto* Title = Cast<UTextBlock>(GetWidgetFromName(TEXT("TitleText"))))
    { Title->SetText(Request.Title); Title->SetVisibility(Request.Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
    if (auto* Message = Cast<UTextBlock>(GetWidgetFromName(TEXT("MessageText")))) Message->SetText(Request.Message);
}

// 平台客户端名称板：只显示调用者事件快照；没有项目资源、GAS、网络或Tick依赖。
#include "WorldUI/GamePlatformWorldNameplateWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UGamePlatformWorldNameplateWidget::ApplyWorldUIRequest(const FGamePlatformWorldUIRequest& InRequest)
{
    Super::ApplyWorldUIRequest(InRequest);
    // 蓝图事件可能同步回收本控件；只投影仍属于本次注册的快照。
    if (GetWorldUIRequest().RequestId != InRequest.RequestId) return;
    if (auto* Fill = Cast<UProgressBar>(GetWidgetFromName(TEXT("ResourceFill"))))
        Fill->SetPercent(static_cast<float>(InRequest.Resource.GetNormalizedValue()));
    if (auto* Name = Cast<UTextBlock>(GetWidgetFromName(TEXT("NameText"))))
        Name->SetText(InRequest.PrimaryText);
    if (auto* Value = Cast<UTextBlock>(GetWidgetFromName(TEXT("ValueText"))))
        Value->SetText(InRequest.SecondaryText);
}

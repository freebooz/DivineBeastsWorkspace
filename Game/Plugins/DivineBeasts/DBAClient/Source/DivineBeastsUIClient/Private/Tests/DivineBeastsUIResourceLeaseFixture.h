#pragma once
// 项目客户端Automation的瞬态根布局类，只为测试提供合法非抽象软类，不创建/保存UI资产。
// 无生产调用方；真正视口挂载由测试显式边界替身控制，不能据此声明Slate或Cook已验收。
#include "Layers/GamePlatformUILayerStack.h"
#include "DivineBeastsUIResourceLeaseFixture.generated.h"

UCLASS(Transient, NotBlueprintable)
class UDivineBeastsUIResourceLeaseRootFixture final : public UGamePlatformUILayerStack
{
    GENERATED_BODY()
};

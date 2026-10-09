// 仅引擎自动化监听夹具：同步执行一次清理回调，模拟外部消费者注销玩家/退出世界，不连接后端。
#pragma once
#include "UObject/Object.h"
#include "QuestSnapshotReentryFixture.generated.h"
UCLASS(Transient, NotBlueprintable)
class UQuestSnapshotReentryFixture : public UObject
{
    GENERATED_BODY()
public:
    /** 用例拥有一次性回调；在调用前移出，重入广播不会递归重复执行。 */
    TFunction<void()> Handler;
    UFUNCTION() void HandleSnapshot() { auto Once = MoveTemp(Handler); if (Once) { Once(); } }
};

// 服务器权威装备契约；仅服务器调用，持久化/资格由注入Port负责；组件退出撤销自有GAS句柄，外部资源不归组件。
#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformEquipmentTypes.h"

using FGamePlatformEquipmentLoadCompletion =
    TFunction<void(
        FGamePlatformEquipmentSnapshot,
        EGamePlatformEquipmentError)>;

using FGamePlatformEquipmentMutationCompletion =
    TFunction<void(
        FGamePlatformEquipmentSnapshot,
        EGamePlatformEquipmentError)>;

class GAMEPLATFORMEQUIPMENTSERVER_API IGamePlatformEquipmentPersistencePort
{
public:
    virtual ~IGamePlatformEquipmentPersistencePort() = default;

    /** 仅服务器游戏线程：已授权PlayerId/CharacterId均非空；false不回调，true在游戏线程恰一次返回完整快照或领域错误；退出的消费者丢弃迟到完成。 */
    virtual bool BeginLoadEquipment(
        const FString& PlayerId,
        const FString& CharacterId,
        FGamePlatformEquipmentLoadCompletion Completion) = 0;

    /** 仅服务器：校验玩家/角色归属、物品所有权、规则、库存/装备版本及OperationId幂等，事务成功后返回完整快照；取消等待不能回滚已提交事务。 */
    virtual bool BeginEquip(
        const FString& PlayerId,
        const FGamePlatformEquipRequest& Request,
        FGamePlatformEquipmentMutationCompletion Completion) = 0;

    /** 仅服务器：非空授权玩家、有效槽位/角色/OperationId及期望版本，落库后回调；并发冲突明确错误，不直接修改客户端。 */
    virtual bool BeginUnequip(
        const FString& PlayerId,
        const FGamePlatformUnequipRequest& Request,
        FGamePlatformEquipmentMutationCompletion Completion) = 0;

    /** 仅服务器：按已授权玩家/角色及原OperationId读取事务结果；false未受理，未知结果明确报错，不能生成新操作冒充恢复。 */
    virtual bool BeginQueryOperationResult(
        const FString& PlayerId,
        const FString& CharacterId,
        const FGuid& OperationId,
        FGamePlatformEquipmentMutationCompletion Completion) = 0;
};

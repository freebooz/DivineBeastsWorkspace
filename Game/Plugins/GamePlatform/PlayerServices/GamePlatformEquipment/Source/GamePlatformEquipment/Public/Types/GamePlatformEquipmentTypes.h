// 平台共享装备纯值契约：服务器持久化/复制与客户端只读视觉共同消费，后端拥有物品与装备权威；不持有GAS或资源句柄。
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GamePlatformEquipmentTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformEquipmentError : uint8
{
    None, // 当前操作无错误，不表示客户端获得权威写权限
    EquipmentNotLoaded, // 尚无可用权威装备快照
    InvalidSlot, // 槽位身份/范围无效
    SlotNotSupported, // 装备定义不支持目标槽位
    ItemNotFound, // 后端或当前有效投影不存在该物品实例
    ItemNotOwned, // 物品实例不属于已授权玩家/角色
    ItemNotEquippable, // 物品规则不允许装备
    ItemAlreadyEquipped, // 同一物品已经在该槽位装备
    ItemEquippedElsewhere, // 物品已经占用其他槽位
    InventoryRevisionConflict, // 背包期望版本冲突，不执行装备事务
    EquipmentRevisionConflict, // 装备期望版本冲突，先对账完整快照
    OperationInProgress, // 已有未决操作，禁止并行修改
    DuplicateOperation, // 操作身份已登记，后端须幂等返回原结果
    GameplayStateProhibitsEquip, // 当前权威玩法状态不允许装备改变
    AbilitySystemUnavailable, // 权威ASC缺失，不能宣布装备玩法就绪
    EquipmentDefinitionMissing, // 所需装备规则定义未加载/不存在
    GameplayGrantFailed, // 本次GAS能力/效果授予失败，撤销本次已授予资源
    PersistenceUnavailable, // 缺少可用持久化Port，未受理权威修改
    PersistenceOutcomeUnknown, // 持久事务可能已提交，不能重放新操作或撤销账本
    RuntimeApplyFailed, // 持久化成功但运行应用失败，明确未就绪并等待对账
    SocketMissing, // 客户端网格缺少定义要求的插槽，仅影响视觉
    VisualLoadFailed, // 客户端视觉资源缺失/加载失败，不改变权威装备
    Unauthorized, // 认证失效或调用方权限不足
    Cancelled, // 仅本地等待取消，不能回滚已提交的权威事务
    TimedOut, // 等待截止时间已到，是否提交依终态与原操作对账
    InvalidResponse // 响应结构/身份/版本无法验证，不能替换旧快照
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMEQUIPMENT_API FGamePlatformEquipmentSlotState
{
    GENERATED_BODY()

    /** 稳定装备槽身份；None无效，同一快照只出现一次。 */
    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FName SlotId = NAME_None;

    /** 后端唯一物品实例身份；空值表示未绑定，装备/背包写请求必须非空。 */
    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FString ItemInstanceId;

    /** 平台中立物品定义身份；None无效，不携带具体项目资产路径。 */
    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FName ItemDefinitionId = NAME_None;

    /** 平台装备规则定义身份；None无效，资源由数据服务/调用方拥有。 */
    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FName EquipmentDefinitionId = NAME_None;

    /** 可选外观定义身份；None表示无装备视觉，不影响服务器权威装备。 */
    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FName VisualDefinitionId = NAME_None;

    /** 服务端条目单调修订号；0初始未知，具体有效范围由所属快照校验。 */
    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    int64 Revision = 0;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMEQUIPMENT_API FGamePlatformPublicEquipmentSlotState
{
    GENERATED_BODY()

    /** 稳定装备槽身份；None无效，同一快照只出现一次。 */
    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FName SlotId = NAME_None;

    /** 平台装备规则定义身份；None无效，资源由数据服务/调用方拥有。 */
    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FName EquipmentDefinitionId = NAME_None;

    /** 可选外观定义身份；None表示无装备视觉，不影响服务器权威装备。 */
    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FName VisualDefinitionId = NAME_None;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMEQUIPMENT_API FGamePlatformEquipmentSnapshot
{
    GENERATED_BODY()

    /** 后端持久角色身份；请求/快照必须匹配当前已授权角色，空值无效。 */
    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    FString CharacterId;

    /** 后端单调装备快照版本；0未加载，有效快照必须为正数。 */
    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    int64 EquipmentRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    TArray<FGamePlatformEquipmentSlotState> Slots;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMEQUIPMENT_API FGamePlatformPublicEquipmentSnapshot
{
    GENERATED_BODY()

    /** 公开装备投影版本；0初始空快照，负值无效。 */
    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    int64 PublicStateRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Equipment")
    TArray<FGamePlatformPublicEquipmentSlotState> Slots;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMEQUIPMENT_API FGamePlatformEquipRequest
{
    GENERATED_BODY()

    /** 全局唯一幂等操作身份；无效Guid拒绝，同一未决写操作恢复使用原身份。 */
    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    FGuid OperationId;

    /** 后端持久角色身份；请求/快照必须匹配当前已授权角色，空值无效。 */
    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    FString CharacterId;

    /** 稳定装备槽身份；None无效，同一快照只出现一次。 */
    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    FName SlotId = NAME_None;

    /** 后端唯一物品实例身份；空值表示未绑定，装备/背包写请求必须非空。 */
    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    FString ItemInstanceId;

    /** 调用方最后读取的装备版本；服务端不匹配时明确拒绝，不覆盖并发修改。 */
    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    int64 ExpectedEquipmentRevision = 0;

    /** 调用方最后读取的背包版本；由服务器事务校验，客户端不能自增代替。 */
    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    int64 ExpectedInventoryRevision = 0;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMEQUIPMENT_API FGamePlatformUnequipRequest
{
    GENERATED_BODY()

    /** 全局唯一幂等操作身份；无效Guid拒绝，同一未决写操作恢复使用原身份。 */
    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    FGuid OperationId;

    /** 后端持久角色身份；请求/快照必须匹配当前已授权角色，空值无效。 */
    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    FString CharacterId;

    /** 稳定装备槽身份；None无效，同一快照只出现一次。 */
    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    FName SlotId = NAME_None;

    /** 调用方最后读取的装备版本；服务端不匹配时明确拒绝，不覆盖并发修改。 */
    UPROPERTY(BlueprintReadWrite, Category="Equipment")
    int64 ExpectedEquipmentRevision = 0;
};

/** 游戏线程纯值预检：字符/版本有效、SlotId唯一、必需物品/装备身份完整；失败整份拒绝，OutReason为脱敏中文原因。
 * 不校验库存所有权或规则资格，这些仍由服务器持久化Port与Definition负责；空Slots表示合法卸空。 */
GAMEPLATFORMEQUIPMENT_API bool ValidateGamePlatformEquipmentSnapshot(const FGamePlatformEquipmentSnapshot& Snapshot, FString& OutReason);
/** 公开复制投影预检：仅检验版本及槽位/装备唯一身份，不读取拥有者私有物品信息。 */
GAMEPLATFORMEQUIPMENT_API bool ValidateGamePlatformPublicEquipmentSnapshot(const FGamePlatformPublicEquipmentSnapshot& Snapshot, FString& OutReason);

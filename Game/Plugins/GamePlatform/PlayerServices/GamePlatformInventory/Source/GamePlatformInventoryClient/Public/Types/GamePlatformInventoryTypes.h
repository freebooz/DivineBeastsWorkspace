#pragma once

#include "CoreMinimal.h"
#include "GamePlatformInventoryTypes.generated.h"

/** EGamePlatformInventoryClientState（背包客户端状态）描述本地只读缓存与单个写事务的生命周期。 */
UENUM(BlueprintType)
enum class EGamePlatformInventoryClientState : uint8
{
    Uninitialized,
    Loading,
    Ready,
    Mutating,
    Reconciling,
    Error
};

/** EGamePlatformInventoryError（背包稳定错误）只由机器错误码映射；禁止解析服务端自然语言消息。 */
UENUM(BlueprintType)
enum class EGamePlatformInventoryError : uint8
{
    None, // 当前操作无错误，不表示客户端获得权威写权限
    InventoryNotLoaded, // 尚无完整背包快照，不能创建写请求
    ItemNotFound, // 后端或当前有效投影不存在该物品实例
    DefinitionNotFound, // 需要的定义身份缺失或未加载
    InvalidQuantity, // 数量非正/超过源物品或业务范围
    StackNotSupported, // 该物品不支持堆叠/拆分
    StackLimitExceeded, // 合并结果超过后端权威堆叠上限
    SlotOutOfRange, // 目标槽位不在权威容器/快捷栏范围
    SlotOccupied, // 目标槽位已有物品，不能覆盖
    ContainerNotFound, // 后端/当前快照不存在目标容器
    RevisionConflict, // 期望版本与权威版本不一致，必须重读
    DuplicateOperation, // 操作身份已登记，后端须幂等返回原结果
    OperationInProgress, // 已有未决操作，禁止并行修改
    OperationNotFound, // 后端明确不存在原操作记录，恢复策略才可按原身份重试
    InventoryFull, // 权威库存没有可用容量
    ConsumeNotAllowed, // 权威规则不允许该消费
    InsufficientQuantity, // 权威库存数量不足
    OutcomeUnknown, // 请求可能已提交，必须按原操作/订单身份查询对账
    BackendUnavailable, // 领域传输/后端不可用，保留可读投影
    Unauthorized, // 认证失效或调用方权限不足
    Cancelled, // 仅本地等待取消，不能回滚已提交的权威事务
    TimedOut, // 等待截止时间已到，是否提交依终态与原操作对账
    InvalidResponse // 响应结构/身份/版本无法验证，不能替换旧快照
};

UENUM()
enum class EGamePlatformInventoryOperationType : uint8
{
    None,
    Move,
    Split,
    Merge,
    SetQuickbar,
    ClearQuickbar
};

/** FGamePlatformInventoryContainerSnapshot（容器快照）描述服务端权威容量边界。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryContainerSnapshot
{
    GENERATED_BODY()

    /** ContainerId（容器编号）是平台中立稳定标识，例如 main；不是项目资产路径。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName ContainerId = NAME_None;

    /** Capacity（容量）表示当前已解锁槽位数，合法槽位范围为 [0, Capacity)。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 Capacity = 0;

    bool IsValid() const
    {
        return !ContainerId.IsNone() && Capacity > 0;
    }
};

/** FGamePlatformInventoryItemInstance（物品实例）是服务端权威长期持有记录的客户端投影。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryItemInstance
{
    GENERATED_BODY()

    /** 后端唯一物品实例身份；空值表示未绑定，装备/背包写请求必须非空。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FString ItemInstanceId;

    /** 平台中立物品定义身份；None无效，不携带具体项目资产路径。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName ItemDefinitionId = NAME_None;

    /** 物品单位整数数量；有效实例/购买请求必须大于0，不使用浮点或本地授予。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 Quantity = 0;

    /** 服务端稳定容器身份；默认main，实际容量来自Containers快照。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName ContainerId = TEXT("main");

    /** 从0开始的槽位索引；INDEX_NONE未绑定，容器操作必须小于权威Capacity，快捷栏小于12。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 SlotIndex = INDEX_NONE;

    /** 服务端条目单调修订号；0初始未知，具体有效范围由所属快照校验。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int64 Revision = 0;

    /** 服务端实例状态投影；默认active，客户端只用于展示与请求预检。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName InstanceState = TEXT("active");

    /** MaxStackSize（权威堆叠上限）来自服务端快照；客户端 Definition 中的同名字段仅用于显示，不可作为规则真源。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 MaxStackSize = 1;

    bool IsValid() const
    {
        return !ItemInstanceId.IsEmpty() &&
               !ItemDefinitionId.IsNone() &&
               Quantity > 0 &&
               !ContainerId.IsNone() &&
               SlotIndex >= 0 &&
               Revision > 0 &&
               MaxStackSize > 0 &&
               Quantity <= MaxStackSize;
    }
};

/** FGamePlatformInventoryQuickbarSlot（快捷栏槽位）只引用已有物品实例，不拥有物品。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryQuickbarSlot
{
    GENERATED_BODY()

    /** 从0开始的槽位索引；INDEX_NONE未绑定，容器操作必须小于权威Capacity，快捷栏小于12。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 SlotIndex = INDEX_NONE;

    /** 后端唯一物品实例身份；空值表示未绑定，装备/背包写请求必须非空。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FString ItemInstanceId;

    /** 服务端条目单调修订号；0初始未知，具体有效范围由所属快照校验。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int64 Revision = 0;
};

/** FGamePlatformInventorySnapshot（背包快照）是客户端唯一长期背包缓存真源。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventorySnapshot
{
    GENERATED_BODY()

    /** 后端单调背包快照版本；0未加载，客户端不能据本地修改自增。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int64 InventoryRevision = 0;

    /** Containers（容器）用于校验槽位容量；项目层不得自行复制第二套容量真源。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    TArray<FGamePlatformInventoryContainerSnapshot> Containers;

    /** 服务端物品实例集合；空集合合法，不由客户端添加权威物品。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    TArray<FGamePlatformInventoryItemInstance> Items;

    /** 快捷栏引用集合；只引用已有物品身份，不另拥有或复制物品。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    TArray<FGamePlatformInventoryQuickbarSlot> Quickbar;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryMutationResult
{
    GENERATED_BODY()

    /** 全局唯一幂等操作身份；无效Guid拒绝，同一未决写操作恢复使用原身份。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FGuid OperationId;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FGamePlatformInventorySnapshot Snapshot;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 MovedQuantity = 0;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 RemainingQuantity = 0;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    bool bDuplicate = false;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryItemDefinitionView
{
    GENERATED_BODY()

    /** 平台中立物品定义身份；None无效，不携带具体项目资产路径。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName ItemDefinitionId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName DisplayNameKey = NAME_None;

    /** 产品说明本地化键；空值未提供，不作为机器错误码。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName DescriptionKey = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName Category = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName IconId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 MaxStackSize = 1;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    TArray<FName> ClientTags;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryItemViewModel
{
    GENERATED_BODY()

    /** 后端唯一物品实例身份；空值表示未绑定，装备/背包写请求必须非空。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FString ItemInstanceId;

    /** 平台中立物品定义身份；None无效，不携带具体项目资产路径。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName ItemDefinitionId = NAME_None;

    /** 物品单位整数数量；有效实例/购买请求必须大于0，不使用浮点或本地授予。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 Quantity = 0;

    /** 服务端稳定容器身份；默认main，实际容量来自Containers快照。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName ContainerId = NAME_None;

    /** MaxStackSize（权威堆叠上限）用于 UI 判断可合并数量；实际写操作仍由服务端再次验证。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 MaxStackSize = 1;

    /** 从0开始的槽位索引；INDEX_NONE未绑定，容器操作必须小于权威Capacity，快捷栏小于12。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 SlotIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    bool bPending = false;

    /** UI/Content层可用此稳定Id解析显示定义；这里不硬引用Widget或资产类。 */
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName DisplayDefinitionHandle = NAME_None;
};

USTRUCT()
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryMoveRequest
{
    GENERATED_BODY()
    FGuid OperationId;
    int64 ExpectedRevision = 0;
    FString ItemInstanceId;
    FName TargetContainerId = TEXT("main");
    int32 TargetSlotIndex = INDEX_NONE;
};

USTRUCT()
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventorySplitRequest
{
    GENERATED_BODY()
    FGuid OperationId;
    int64 ExpectedRevision = 0;
    FString SourceItemInstanceId;
    int32 SplitQuantity = 0;
    FName TargetContainerId = TEXT("main");
    int32 TargetSlotIndex = INDEX_NONE;
};

USTRUCT()
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryMergeRequest
{
    GENERATED_BODY()
    FGuid OperationId;
    int64 ExpectedRevision = 0;
    FString SourceItemInstanceId;
    FString TargetItemInstanceId;
};

USTRUCT()
struct GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryQuickbarRequest
{
    GENERATED_BODY()
    FGuid OperationId;
    int64 ExpectedRevision = 0;
    int32 SlotIndex = INDEX_NONE;
    FString ItemInstanceId;
};

#include "Screens/Inventory/DivineBeastsInventoryTextCatalog.h"

// 迁移后沿用原页面的翻译命名空间与精确键，不破坏已制作的本地化资源身份。
#define LOCTEXT_NAMESPACE "DivineBeastsInventoryScreen"

FText FDivineBeastsInventoryTextCatalog::ForState(EGamePlatformInventoryClientState State)
{
    switch (State)
    {
    case EGamePlatformInventoryClientState::Uninitialized:
        return LOCTEXT("InventoryUninitialized", "尚未登录或背包未初始化");
    case EGamePlatformInventoryClientState::Loading:
        return LOCTEXT("InventoryLoading", "正在读取背包…");
    case EGamePlatformInventoryClientState::Ready:
        return LOCTEXT("InventoryReady", "背包已同步");
    case EGamePlatformInventoryClientState::Mutating:
        return LOCTEXT("InventoryMutating", "正在提交操作…");
    case EGamePlatformInventoryClientState::Reconciling:
        return LOCTEXT("InventoryReconciling", "正在核对服务器结果…");
    case EGamePlatformInventoryClientState::Error:
        return LOCTEXT("InventoryError", "背包暂时不可用");
    default:
        return LOCTEXT("InventoryUnknown", "背包状态未知");
    }
}

FText FDivineBeastsInventoryTextCatalog::ForError(EGamePlatformInventoryError Error)
{
    // 不使用后端自然语言，始终以稳定机器错误码选择已批准的项目中文文本。
    switch (Error)
    {
    case EGamePlatformInventoryError::None: return FText::GetEmpty();
    case EGamePlatformInventoryError::InventoryNotLoaded:
        return LOCTEXT("InventoryNotLoaded", "背包尚未加载");
    case EGamePlatformInventoryError::ItemNotFound:
        return LOCTEXT("InventoryItemNotFound", "物品不存在或已变化，请刷新背包");
    case EGamePlatformInventoryError::DefinitionNotFound:
        return LOCTEXT("InventoryDefinitionNotFound", "物品显示资料不可用");
    case EGamePlatformInventoryError::InvalidQuantity:
        return LOCTEXT("InventoryInvalidQuantity", "物品数量不合法");
    case EGamePlatformInventoryError::StackNotSupported:
        return LOCTEXT("InventoryStackNotSupported", "该物品不支持拆分或堆叠");
    case EGamePlatformInventoryError::StackLimitExceeded:
        return LOCTEXT("InventoryStackLimitExceeded", "超出物品堆叠上限");
    case EGamePlatformInventoryError::SlotOutOfRange:
        return LOCTEXT("InventorySlotOutOfRange", "背包槽位超出当前容量");
    case EGamePlatformInventoryError::SlotOccupied:
        return LOCTEXT("InventorySlotOccupied", "目标位置已有物品");
    case EGamePlatformInventoryError::ContainerNotFound:
        return LOCTEXT("InventoryContainerNotFound", "背包容器不存在");
    case EGamePlatformInventoryError::RevisionConflict:
        return LOCTEXT("InventoryRevisionConflict", "背包已更新，正在重新同步");
    case EGamePlatformInventoryError::DuplicateOperation:
        return LOCTEXT("InventoryDuplicateOperation", "该背包操作已提交");
    case EGamePlatformInventoryError::OperationInProgress:
        return LOCTEXT("InventoryOperationInProgress", "上一项背包操作尚未完成");
    case EGamePlatformInventoryError::OperationNotFound:
        return LOCTEXT("InventoryOperationNotFound", "服务器未找到原操作，正在恢复");
    case EGamePlatformInventoryError::InventoryFull:
        return LOCTEXT("InventoryFull", "背包空间不足");
    case EGamePlatformInventoryError::ConsumeNotAllowed:
        return LOCTEXT("InventoryConsumeNotAllowed", "当前物品不可使用");
    case EGamePlatformInventoryError::InsufficientQuantity:
        return LOCTEXT("InventoryInsufficientQuantity", "物品数量不足");
    case EGamePlatformInventoryError::OutcomeUnknown:
        return LOCTEXT("InventoryOutcomeUnknown", "结果尚未确认，请查询原操作，勿重复提交");
    case EGamePlatformInventoryError::BackendUnavailable:
        return LOCTEXT("InventoryBackendUnavailable", "背包服务器暂不可用");
    case EGamePlatformInventoryError::Unauthorized:
        return LOCTEXT("InventoryUnauthorized", "登录失效或操作无权限");
    case EGamePlatformInventoryError::Cancelled:
        return LOCTEXT("InventoryCancelled", "已取消等待，服务器结果仍需核对");
    case EGamePlatformInventoryError::TimedOut:
        return LOCTEXT("InventoryTimedOut", "请求超时，请核对原操作");
    case EGamePlatformInventoryError::InvalidResponse:
        return LOCTEXT("InventoryInvalidResponse", "背包数据校验失败");
    default:
        return LOCTEXT("InventoryUnknownError", "背包操作失败");
    }
}

#undef LOCTEXT_NAMESPACE

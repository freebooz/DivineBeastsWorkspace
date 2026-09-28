#include "Interfaces/IGamePlatformOnlineService.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, GamePlatformOnline)

IGamePlatformOnlineService* IGamePlatformOnlineService::Get(
    UGameInstance&)
{
    // 当前平台门面尚未装配生产子系统；必须显式返回不可用，使上层走失败路径，
    // 不能为通过链接而伪造已认证状态、令牌或成功响应。
    return nullptr;
}

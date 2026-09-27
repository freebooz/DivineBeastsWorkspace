#include "Modules/ModuleManager.h"

/** DBAWorldsRuntime（项目世界定义模块）仅注册可用类型，不在启动时加载地图或连接后端。 */
class FDBAWorldsRuntimeModule final : public IModuleInterface
{
};

IMPLEMENT_MODULE(FDBAWorldsRuntimeModule, DBAWorldsRuntime)

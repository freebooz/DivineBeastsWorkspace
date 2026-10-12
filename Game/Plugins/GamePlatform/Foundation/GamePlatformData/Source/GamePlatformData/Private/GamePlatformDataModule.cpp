// 平台层双端模块只登记进程元数据索引，不加载业务资产、不拥有世界或PIE状态。
// 关闭按同一所有权显式解绑Registry事件并清空索引，原AssetManager与租约生命周期不变。
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Validation/GamePlatformDefinitionSourceIndex.h"

class FGamePlatformDataModule final : public IModuleInterface
{
public:
    /** 仅GameThread；注册事件后保持冷态，首次就绪查询才读取源资产快照。 */
    virtual void StartupModule() override
    {
        check(IsInGameThread());
        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        GamePlatform::Data::GetDefinitionSourceIndex().Startup(Registry);
    }

    /** 仅GameThread；先取消未发布重建，再解绑并清理，不在退出阶段重新加载Registry。 */
    virtual void ShutdownModule() override
    {
        GamePlatform::Data::GetDefinitionSourceIndex().Shutdown();
    }
};

IMPLEMENT_MODULE(FGamePlatformDataModule, GamePlatformData)

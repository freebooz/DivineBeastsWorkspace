#pragma once
#include "Engine/AssetManager.h"
#include "Types/GamePlatformResult.h"
#include "GamePlatformAssetManager.generated.h"

class UGamePlatformDataSubsystem;
struct FGamePlatformProcessDemands;

/** 在主工程配置一次的引擎资产管理器。进程登记表私有，业务必须通过实例服务登记租约。 */
UCLASS()
class GAMEPLATFORMDATA_API UGamePlatformAssetManager : public UAssetManager
{
    GENERATED_BODY()
public:
    /** 创建进程需求账本，不加载业务内容。 */
    UGamePlatformAssetManager();
    /** 在完整私有类型可见处销毁账本，避免反射生成文件实例化不完整类型。 */
    virtual ~UGamePlatformAssetManager() override;
    /** 热重载构造同样在实现文件中定义，保持不完整类型边界。 */
    UGamePlatformAssetManager(FVTableHelper& Helper);
    using UAssetManager::ChangeBundleStateForPrimaryAssets;
    /**
     * 引擎公开主资产加载最终经过此虚函数。接纳后来的外部需求为保守基线，并合并仍存活租约；
     * 即使同分组无新句柄也登记，外部请求不能移除本服务仍需要的分组。只在游戏线程调用。
     */
    virtual TSharedPtr<FStreamableHandle> ChangeBundleStateForPrimaryAssets(
        const TArray<FPrimaryAssetId>& AssetsToChange, const TArray<FName>& AddBundles,
        const TArray<FName>& RemoveBundles, bool bRemoveAllBundles, FAssetManagerLoadParams&& LoadParams,
        UE::FSourceLocation Location = UE::FSourceLocation::Current()) override;
    /** 引擎公开卸载入口：有本服务租约的资产拒绝卸载，其余交回引擎；返回实际卸载数量。 */
    virtual int32 UnloadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToUnload) override;
private:
    friend class UGamePlatformDataSubsystem;
    TUniquePtr<FGamePlatformProcessDemands> ProcessDemands;
    FGamePlatformResult AddDemand(const FPrimaryAssetId& AssetId, const FString& LeaseKey,
        const TArray<FName>& Bundles, TFunction<void(FGamePlatformResult)> Completion);
    void RemoveDemand(const FPrimaryAssetId& AssetId, const FString& LeaseKey);
    void Reconcile(const FPrimaryAssetId& AssetId);
    void CompleteReconcile(FPrimaryAssetId AssetId, FGuid Serial);
};

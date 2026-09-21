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
private:
    friend class UGamePlatformDataSubsystem;
    TUniquePtr<FGamePlatformProcessDemands> ProcessDemands;
    FGamePlatformResult AddDemand(const FPrimaryAssetId& AssetId, const FString& LeaseKey,
        const TArray<FName>& Bundles, TFunction<void(FGamePlatformResult)> Completion);
    void RemoveDemand(const FPrimaryAssetId& AssetId, const FString& LeaseKey);
    void Reconcile(const FPrimaryAssetId& AssetId);
    void CompleteReconcile(FPrimaryAssetId AssetId, FGuid Serial);
};

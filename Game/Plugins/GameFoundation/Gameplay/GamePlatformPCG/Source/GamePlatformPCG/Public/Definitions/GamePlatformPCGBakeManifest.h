#pragma once
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "GamePlatformPCGBakeManifest.generated.h"

/** 静态交付来源与实际输出记录；不包含图/材质硬引用，不构成网络授权。 */
UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGBakeManifest final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    /** 当前世界与区域稳定身份，不从客户端网络地址推导。 */
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="PCG") FGamePlatformId WorldId;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="PCG") FGamePlatformId RegionId;
    /** 原配置主资产ID及真实内容修订；清单自身还有独立DataVersion。 */
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="PCG") FPrimaryAssetId ProfileId;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="PCG") int32 ProfileRevision = 0;
    /** 实际源文件指纹，算法/依赖范围另记；不与平台Cook二进制哈希混用。 */
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="PCG") FString SourceFingerprint;
    /** 排序规范化后的实际ISM变换摘要；毫米平移、万分之一旋转/缩放量化。 */
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="PCG") FString OutputFingerprint;
    /** 指纹覆盖的源包路径与校验值，未知依赖不能静默省略。 */
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="PCG") TArray<FString> SourceDependencies;
    /** 真实实例数，不是组件数。 */
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="PCG") int32 InstanceCount = 0;
    /** 输出所属Actor精确软路径；运行时释放不得据此删除编辑器保存的静态资源。 */
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="PCG") FSoftObjectPath OwnedOutput;
    /** 保存时引擎版本/插件工具版本，用于重现条件，不隐含跨版本一致。 */
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="PCG") FString GeneratorVersion;
    /** 只有独立关闭重开和碰撞检查完成后才可批准；生成/保存本身不得自动设真。 */
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="PCG") bool bIsReopenVerified = false;
    /** 公开校验仅检查清单完整性，不证明场景碰撞或服务器授权。 */
    virtual FGamePlatformResult ValidateDefinition() const override;
};

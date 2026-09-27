#pragma once
#include "Definitions/GamePlatformDefinitionBase.h"
#include "PCGGraph.h"
#include "Engine/StaticMesh.h"
#include "GamePlatformPCGProfileDefinition.generated.h"

/** 发布静态结果不允许运行期重生成；运行时权威模式明确不支持。 */
UENUM(BlueprintType)
enum class EGamePlatformPCGExecutionPolicy : uint8 { EditorGeneratedStatic, RuntimeCosmetic, RuntimeAuthoritative };
/** 纯装饰必须关闭全部碰撞、重叠、导航和复制；静态阻挡只允许编辑器生成后交付。 */
UENUM(BlueprintType)
enum class EGamePlatformPCGOutputUsage : uint8 { Cosmetic, StaticCollision };

/** 有界平面体积网格配置；身份和版本只复用Data，运行时不修改源资产。 */
UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGProfileDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    /** 仅认可经本插件白名单审查的原生图；Graph分组由Data保留到清理完成。 */
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="PCG",meta=(AssetBundles="PCGGeneration"))
    TSoftObjectPtr<UPCGGraph> GraphReference;
    /** 白名单唯一网格；不从图内任意路径偷偷加载新资源。 */
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="PCG",meta=(AssetBundles="PCGGeneration"))
    TSoftObjectPtr<UStaticMesh> OutputMesh;
    /** 编辑器静态与运行时装饰互斥；RuntimeAuthoritative永远Unsupported。 */
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="PCG")
    EGamePlatformPCGExecutionPolicy ExecutionPolicy = EGamePlatformPCGExecutionPolicy::RuntimeCosmetic;
    /** 输出用途决定生成前后双重属性检查，不能单靠bCosmetic放行。 */
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="PCG")
    EGamePlatformPCGOutputUsage OutputUsage = EGamePlatformPCGOutputUsage::Cosmetic;
    /** World中已存在的逻辑区域；PCG不自行注册第二套区域目录。 */
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="PCG") FGamePlatformId RegionId;
    /** 本地轴对齐体积半径，单位厘米，各轴必须正且不超过5000。当前只支持水平平面体积采样。 */
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="PCG") FVector HalfExtentCm = FVector(500,500,50);
    /** 网格最小间距厘米；密度降低时扩大间距，不做独立随机摆放。 */
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="PCG") double SpacingCm = 100;
    /** 0..1覆盖率；0以原生过滤节点得到合法零输出，非零间距按1/sqrt(Density)放大。 */
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="PCG") double Density = 1;
    /** 单一均匀缩放，有限且0..10；不允许任意属性覆盖或副作用蓝图。 */
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="PCG") double UniformScale = 1;
    /** 与稳定世界/区域/配置身份和修订一起派生原生种子，不作为跨端一致性证明。 */
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="PCG") int32 Seed = 1;
    /** 0表示允许空结果；实际数量不足不得把图错误当空区域成功。 */
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="PCG") int32 MinimumOutputs = 0;
    /** 初始测试硬上限4096，不代表移动端性能达标。 */
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="PCG") int32 MaximumOutputs = 4096;
    /** 包括加载/生成/验证的单调时钟截止，秒；清理超时保持Cleaning而不谎报释放。 */
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="PCG") double TimeoutSeconds = 30;
    /** 字段校验不加载图，不执行生成；不支持的策略明确Unsupported。 */
    virtual FGamePlatformResult ValidateDefinition() const override;
};

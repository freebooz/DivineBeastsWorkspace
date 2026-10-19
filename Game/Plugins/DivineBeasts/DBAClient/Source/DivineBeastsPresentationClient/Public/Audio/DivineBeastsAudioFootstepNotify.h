// SSLM脚步动画通知：客户端脚掌落地时按真实物理材质提交SFX语义，不影响角色移动或权威物理。
#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Engine/EngineTypes.h" // 物理表面/可配置追踪通道枚举。
#include "DivineBeastsAudioFootstepNotify.generated.h"

/**
 * 按动画脚掌触地帧显式配置；无Tick，无SoundWave硬引用。
 * 根据命中PhysicalMaterial资产名匹配音频库中的Footstep.*，未命中使用项目指定回退。
 */
UCLASS(meta=(DisplayName="SSLM Audio Footstep"))
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsAudioFootstepNotify : public UAnimNotify
{
    GENERATED_BODY()
public:
    /** 足底Socket名称；不存在时从Mesh位置向下追踪。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SSLM|Audio|Footstep")
    FName FootSocketName = NAME_None;
    /** 未命中物理材质时的音效语义；与平台SFX资源路径无关。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SSLM|Audio|Footstep")
    FName DefaultClipId = TEXT("Footstep.LeavesA");
    /** PhysicalMaterial（物理材质）资产FName到Footstep.*语义的覆盖表。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SSLM|Audio|Footstep")
    TMap<FName, FName> PhysicalMaterialToClip;
    /** UE物理表面类型的语义覆盖；仅在资产名未命中时使用，不预设未经核实的村庄材质。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SSLM|Audio|Footstep")
    TMap<TEnumAsByte<EPhysicalSurface>, FName> SurfaceTypeToClip;
    /** 可依据真实地面碰撞设置调整，默认使用可见性通道。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SSLM|Audio|Footstep")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;
    /** 仅客户端脚步检测，单位厘米，限制允许编辑值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SSLM|Audio|Footstep", meta=(ClampMin="0", ClampMax="100"))
    float TraceUpCm = 25.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SSLM|Audio|Footstep", meta=(ClampMin="30", ClampMax="300"))
    float TraceDownCm = 150.f;

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
};

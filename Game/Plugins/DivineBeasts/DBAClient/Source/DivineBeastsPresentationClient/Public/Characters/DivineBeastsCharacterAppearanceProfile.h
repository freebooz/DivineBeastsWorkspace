#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "DivineBeastsCharacterAppearanceProfile.generated.h"

class UAnimInstance;
class UMaterialInterface;
class USkeletalMesh;

/**
 * UDivineBeastsCharacterAppearanceProfile（神兽联盟角色外观配置）。
 * 仅在客户端表现模块使用，把稳定HeroDefinitionId映射到可替换的Mesh、材质和动画资源。
 *
 * 设计边界：
 * - 本资产不是服务器权威定义，不保存伤害、移动、碰撞、技能或资格数据；
 * - 所有视觉资源均使用软引用，Dedicated Server不依赖本类型；
 * - 占位Manny/Quinn与未来真实生肖模型复用同一Profile资产身份，替换资源即可，不迁移存档和协议。
 */
UCLASS(BlueprintType)
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsCharacterAppearanceProfile
    : public UDataAsset
{
    GENERATED_BODY()

public:
    /** 稳定外观逻辑ID，例如Appearance.Hero.Zodiac.Rat.Default。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Character|Appearance")
    FName ProfileId = NAME_None;

    /** Shared契约中的稳定Hero Definition ID，例如Hero.Zodiac.Rat。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Character|Appearance")
    FName HeroDefinitionId = NAME_None;

    /** 骨架兼容逻辑ID；用于替换真实角色时显式检查动画兼容边界。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Character|Appearance")
    FName SkeletonCompatibilityId = NAME_None;

    /** 客户端角色骨骼网格软引用；原型阶段指向Manny/Quinn，正式阶段指向生肖模型。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Character|Appearance")
    TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

    /** 按材质槽顺序覆盖；原型阶段使用生肖唯一颜色材质，正式阶段替换为真实角色材质。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Character|Appearance")
    TArray<TSoftObjectPtr<UMaterialInterface>> MaterialOverrides;

    /** 可选动画实例类软引用；为空表示由更高层动画Profile或角色装配决定。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Character|Appearance")
    TSoftClassPtr<UAnimInstance> AnimInstanceClass;

    /** Mesh挂到标准ACharacter Mesh组件后的相对位置；Manny/Quinn默认使用Z=-90cm。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Character|Appearance")
    FVector MeshRelativeLocation = FVector(0.0, 0.0, -90.0);

    /** Mesh相对旋转；UE5 Mannequin默认面向角色前方需要Yaw=-90度。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Character|Appearance")
    FRotator MeshRelativeRotation = FRotator(0.0, -90.0, 0.0);

    /** Mesh相对缩放；正式生肖模型可在Profile内单独调整，不污染角色权威碰撞。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Character|Appearance")
    FVector MeshRelativeScale = FVector::OneVector;

    /** 资产结构版本；只用于客户端外观兼容，不替代Server-safe Hero Definition版本。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Character|Appearance", meta=(ClampMin="1"))
    int32 Version = 1;

    /** 客户端外观内容修订号，用于资源预加载和诊断。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Character|Appearance")
    FString ContentRevision = TEXT("1");

    /** 开发占位标记；正式生肖美术替换完成后必须关闭。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Character|Appearance")
    bool bDevelopmentPlaceholder = false;

    /** 仅用于编辑器/诊断识别生肖占位色，不参与权威玩法。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Character|Appearance")
    FLinearColor DevelopmentTint = FLinearColor::White;

    /** 纯字段校验，不同步加载软资源。 */
    bool IsProfileValid(FString& OutError) const;
};

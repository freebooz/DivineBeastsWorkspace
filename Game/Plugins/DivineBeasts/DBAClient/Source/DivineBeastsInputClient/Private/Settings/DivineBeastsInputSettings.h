#pragma once

#include "Engine/DeveloperSettings.h"
#include "UObject/PrimaryAssetId.h"
#include "DivineBeastsInputSettings.generated.h"

/**
 * UDivineBeastsInputSettings（神兽联盟输入项目设置）。
 *
 * 这是项目层唯一的默认输入装配配置，只引用真实 Data Definition（数据定义）身份和 Context 名称；
 * 不保存键盘键、手柄按钮、触控坐标或技能业务逻辑，这些仍属于 Input Profile（输入配置）资产。
 *
 * 默认 ProfileId 为空，因此源码不会虚构不存在的输入资产；内容团队生成并登记正式 Profile 后才自动启用。
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="神兽联盟输入"))
class UDivineBeastsInputSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** 是否允许 LocalPlayer 子系统在初始化后自动准备默认Gameplay输入。 */
    UPROPERTY(Config, EditAnywhere, Category="Input")
    bool bAutoActivateGameplayInput = true;

    /** 神兽联盟默认Gameplay Input Profile的Primary Asset身份；空值表示尚未配置正式资产。 */
    UPROPERTY(Config, EditAnywhere, Category="Input")
    FPrimaryAssetId DefaultGameplayProfileId;

    /** 需要从Profile申请的Mapping Context逻辑名称；不得重复。 */
    UPROPERTY(Config, EditAnywhere, Category="Input")
    TArray<FName> GameplayContextNames;

    /** 项目Gameplay Context优先级，平台服务仍会验证0..100安全范围。 */
    UPROPERTY(Config, EditAnywhere, Category="Input", meta=(ClampMin="0", ClampMax="100"))
    int32 GameplayContextPriority = 50;

    /** 本地用户输入偏好稳定命名空间；不会作为账号或认证身份使用。 */
    UPROPERTY(Config, EditAnywhere, Category="Input")
    FString LocalSettingsKey = TEXT("DivineBeasts.Default");
};

// 项目层客户端创建介绍主资产；由DBAUIPack_Core拥有内容，VM通过GamePlatformData普通资源租约读取。
// 不参与GAS授权，不持有数值/AbilitySet，不以草案标记改变服务器英雄或技能资格。
#pragma once
#include "Definitions/DivineBeastsAbilityUIProfile.h"
#include "DivineBeastsCharacterCreationProfile.generated.h"

/** 复用现有技能显示条目，但独立于战斗UIProfile注册身份的创建页纯内容配置。 */
UCLASS(BlueprintType)
class DIVINEBEASTSUICLIENT_API UDivineBeastsCharacterCreationProfile : public UDivineBeastsAbilityUIProfile
{
    GENERATED_BODY()
public:
    /** 默认按未批准草案显示；正式内容须独立审核后设置false，不能仅凭资产位于正式目录解除声明。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|CharacterCreation")
    bool bDescriptionsAreDraft = true;

    /** 主资产类型独立为DivineBeastsCharacterCreationProfile，名称仍为既有HeroDefinitionId。 */
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    /** 游戏线程或只读作者审核调用；先复用父类校验，再检查五项介绍、同英雄身份和草案声明。
     * 不加载软资源，失败写中文OutError，成功清空；不判断技能是否被服务器授予。 */
    bool ValidateCreationProfile(FString& OutError) const;
};

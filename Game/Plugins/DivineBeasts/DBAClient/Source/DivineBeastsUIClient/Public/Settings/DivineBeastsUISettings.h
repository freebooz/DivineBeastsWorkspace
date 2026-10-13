// 项目客户端UI只读配置；配置所有权属于项目装配，VM只取软路径，不保存玩家输入或业务权威。
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "DivineBeastsUISettings.generated.h"

/** 直接采用UObject Config合同，不增加通用设置服务或编辑器模块依赖。 */
UCLASS(Config=Game, DefaultConfig)
class DIVINEBEASTSUICLIENT_API UDivineBeastsUISettings : public UObject
{
    GENERATED_BODY()
public:
    /** 同英雄ID到创建介绍DataAsset对象软路径；只允许DBAUIPack_Core/UI/Profiles/CharacterCreation。
     * 不引用/Game/Development、AbilitySet或权威数值。映射缺失/非法时明确失败，不猜路径补造内容。 */
    UPROPERTY(Config, EditAnywhere, Category="DivineBeasts|UI|CharacterCreation")
    TMap<FName, FSoftObjectPath> CharacterCreationProfiles;

    /** 只读解析；成功填写OutPath并清空OutError，失败清空OutPath并说明原因，不发起加载。 */
    bool TryGetCharacterCreationProfilePath(FName HeroDefinitionId, FSoftObjectPath& OutPath, FString& OutError) const;
};

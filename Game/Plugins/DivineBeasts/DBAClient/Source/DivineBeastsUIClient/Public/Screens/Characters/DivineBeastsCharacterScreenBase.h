#pragma once

#include "Screens/DivineBeastsUIScreen.h"
#include "DivineBeastsCharacterScreenBase.generated.h"

/** 角色创建/选择页面的共同只读投影：不复制资格数据，不执行角色创建权威判定。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsCharacterScreenBase : public UDivineBeastsUIScreen
{
    GENERATED_BODY()
public:
    UDivineBeastsCharacterScreenBase() { UIDomain = EDivineBeastsUIDomain::Character; }

    /** 当前账号持久角色数量，无状态源时为0。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Characters")
    int32 GetPersistentCharacterCount() const;
    /** 已确认可展示的生肖候选数量，非最终创建权限。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Characters")
    int32 GetEligibleHeroCount() const;
    /** 当前选择的持久角色标识，非服务器准入票据。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Characters")
    FString GetSelectedPersistentCharacterId() const;
};

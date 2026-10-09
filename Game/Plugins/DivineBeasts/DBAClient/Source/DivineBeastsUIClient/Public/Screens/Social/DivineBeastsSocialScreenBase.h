#pragma once

#include "Contracts/Domains/DivineBeastsSocialUIContracts.h"
#include "Screens/DivineBeastsUIScreen.h"
#include "DivineBeastsSocialScreenBase.generated.h"

class UDivineBeastsSocialViewModel;

/** 社交与组队页面基类：显示授权快照，交互命令由独立社交适配器处理。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsSocialScreenBase : public UDivineBeastsUIScreen
{
    GENERATED_BODY()
public:
    UDivineBeastsSocialScreenBase() { UIDomain = EDivineBeastsUIDomain::Social; }
    /** 类型安全地取得当前社交ViewModel。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Social")
    UDivineBeastsSocialViewModel* GetSocialViewModel() const;
    /** 服务未接入时返回不可用的空数据，而非伪造好友或队友。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Social")
    FDivineBeastsSocialUIProjection GetSocialProjection() const;
};

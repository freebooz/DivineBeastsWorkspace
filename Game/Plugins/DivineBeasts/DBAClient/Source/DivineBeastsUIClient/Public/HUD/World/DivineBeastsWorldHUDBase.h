#pragma once

#include "Contracts/DivineBeastsUIContracts.h"
#include "HUD/DivineBeastsHUDWidget.h"
#include "DivineBeastsWorldHUDBase.generated.h"

/** 开放世界、新手村、教学、训练HUD统一只读视图，不增加新的世界状态数据源。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsWorldHUDBase : public UDivineBeastsHUDWidget
{
    GENERATED_BODY()
public:
    /** 从平台已绑定的项目ViewModel获取当前世界状态。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|WorldHUD")
    FDivineBeastsUIWorldProjection GetWorldProjection() const;
    /** 已具备可展示的WorldId时返回true。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|WorldHUD")
    bool HasWorldView() const;
};

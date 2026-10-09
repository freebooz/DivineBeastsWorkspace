#pragma once

#include "Contracts/DivineBeastsUIContracts.h"
#include "Screens/DivineBeastsUIScreen.h"
#include "DivineBeastsWorldScreenBase.generated.h"

/** 世界与任务页面的共同只读视图：不扫描Actor、不执行Travel与世界准入。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsWorldScreenBase : public UDivineBeastsUIScreen
{
    GENERATED_BODY()
public:
    UDivineBeastsWorldScreenBase() { UIDomain = EDivineBeastsUIDomain::World; }
    /** 从原项目视图模型读取当前世界投影，缺少时返回空快照。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|World")
    FDivineBeastsUIWorldProjection GetWorldProjection() const;
    /** WorldId有效表示可展示世界信息，不表示专用服务器已完成准入。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|World")
    bool HasWorldView() const;
};

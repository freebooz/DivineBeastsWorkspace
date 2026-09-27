#pragma once

#include "HUD/DivineBeastsHUDWidget.h"
#include "DivineBeastsArenaHUD.generated.h"

/**
 * UDivineBeastsArenaHUD（神兽联盟主竞技场 HUD C++ 基类）。
 *
 * 用于 MainArena（主竞技场）1v1～5v5 模式。
 * HUD 只显示服务器/复制状态确认后的生命、比分、阶段、时间等事实；
 * PC 与 Mobile 可使用不同 Widget Blueprint，但必须共享同一 ViewModel 和事件语义。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsArenaHUD
    : public UDivineBeastsHUDWidget
{
    GENERATED_BODY()

public:
    UDivineBeastsArenaHUD()
    {
        HUDKind = EDivineBeastsHUDKind::Arena;
        HUDSurfaceId = TEXT("UI.HUD.Arena");
    }
};

#pragma once

#include "Screens/GamePlatformHUDWidget.h"
#include "DivineBeastsHUDWidget.generated.h"

/** EDivineBeastsHUDKind（神兽联盟 HUD 类型）。 */
UENUM(BlueprintType)
enum class EDivineBeastsHUDKind : uint8
{
    /** 开放世界主 HUD。 */
    OpenWorld,

    /** 新手村主 HUD。 */
    Village,

    /** 教学引导 HUD。 */
    Tutorial,

    /** 训练控制 HUD。 */
    Training,

    /** 主竞技场 HUD。 */
    Arena
};

/**
 * UDivineBeastsHUDWidget（神兽联盟 HUD 基类）。
 *
 * 职责：
 * - 统一承接平台 HUD 的事件驱动 ViewModel 与 PC / Mobile 自适应能力。
 * - 为五类项目 HUD 提供稳定 HUDKind 和 SurfaceId 身份。
 * - HUD 本身只显示已授权的只读投影，不直接扫描 ASC、Actor 或后端对象。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsHUDWidget
    : public UGamePlatformHUDWidget
{
    GENERATED_BODY()

public:
    /** 返回当前 HUD 的项目分类。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|HUD")
    EDivineBeastsHUDKind GetHUDKind() const { return HUDKind; }

    /** 返回当前 HUD 的稳定 SurfaceId（界面表面标识）。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|HUD")
    FName GetHUDSurfaceId() const { return HUDSurfaceId; }

protected:
    /**
     * HUD 构造完成后绑定项目 ViewModel 的只读状态事件。
     * 平台 ViewModel 与 Adaptive 事件由父类负责，本函数只处理项目层订阅。
     */
    virtual void BindUIEvents() override;

    /** HUD 销毁前解绑项目 ViewModel，避免不可见 HUD 持续接收状态事件。 */
    virtual void UnbindUIEvents() override;

    /** 由具体 HUD 子类在构造函数中设置，不在运行时频繁修改。 */protected:
    /** 由具体 HUD 子类在构造函数中设置，不在运行时频繁修改。 */
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|UI|HUD")
    EDivineBeastsHUDKind HUDKind = EDivineBeastsHUDKind::OpenWorld;

    /** 与项目 UI Catalog / Definition 对齐的稳定界面标识。 */
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|UI|HUD")
    FName HUDSurfaceId = NAME_None;
};

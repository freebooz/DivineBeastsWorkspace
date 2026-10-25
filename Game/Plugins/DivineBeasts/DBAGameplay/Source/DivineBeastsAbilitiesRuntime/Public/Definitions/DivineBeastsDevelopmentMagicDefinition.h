// 项目层双端开发魔法数据，派生既有平台Definition链；不含图标、Niagara或MOBA资源。
#pragma once
#include "Definitions/DivineBeastsAbilityDefinition.h"
#include "Types/GamePlatformCombatProjectileMotion.h"
#include "DivineBeastsDevelopmentMagicDefinition.generated.h"

/** 仅用于暂时开发测试的三种原创法术语义；非已取消的五行玩法及正式角色职业。 */
UENUM(BlueprintType)
enum class EDivineBeastsDevelopmentMagicKind : uint8 { Fireball, Frostbolt, ArcaneMissiles };

/** 保留英雄/逻辑ID/版本/数值表合同；差异使用资产实例，不创建三个技能插件或三份伤害执行器。 */
UCLASS(BlueprintType)
class DIVINEBEASTSABILITIESRUNTIME_API UDivineBeastsDevelopmentMagicDefinition : public UDivineBeastsAbilityDefinition
{
    GENERATED_BODY()
public:
    UDivineBeastsDevelopmentMagicDefinition();
    /** SSLM法术恒速合同，厘米/秒；起飞及全程共用同值，不改变平台对其他游戏的通用上界。 */
    static constexpr float MinProjectileSpeedCmPerSecond = 3000.0f;
    static constexpr float DefaultProjectileSpeedCmPerSecond = 4000.0f;
    static constexpr float MaxProjectileSpeedCmPerSecond = 5000.0f;
    /** 拒绝范围外及非有限速度；仅项目服务器的技能数据校验使用，不接受客户端覆盖。 */
    static bool IsProjectileSpeedSupported(float SpeedCmPerSecond);
    /** 火球/冰箭默认权威起手（秒）；开发与占位执行器共用，同时驱动施法反馈与首次释放Timer。
     * 不套用于奥术连发、定向飞弹及已批准的独立技能例外，也不新增客户端计时真源。 */
    static constexpr float DefaultCastDurationSeconds = 0.5f;
    /** 项目外观语义；服务器仅复制分类身份，不加载对应表现资产。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|DevelopmentMagic")
    EDivineBeastsDevelopmentMagicKind MagicKind = EDivineBeastsDevelopmentMagicKind::Fireball;
    /** 平台中立运动参数；恒速默认4000、允许3000..5000厘米/秒，实际射程必须与BalanceRow一致。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|DevelopmentMagic")
    FGamePlatformCombatProjectileMotion ProjectileMotion;
    /** 同一次GAS成本/冷却Commit的弹数，1..3；每枚使用独立发生ID并各自结算。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|DevelopmentMagic")
    int32 ProjectileCount = 1;
    /** 连发间隔秒，单发0、连发[0.05,1]；取消/死亡/旅行撤销尚未释放的定时器。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|DevelopmentMagic")
    float ProjectileIntervalSeconds = 0.0f;
    /** 父类静态身份合同加有限运动/有界连发校验；不在此加载数值表或资源。 */
    virtual FGamePlatformResult ValidateDefinition() const override;
    /** 项目允许列表到中立不透明身份；无效枚举返回None，不拼任意路径。 */
    FName GetPresentationStyleId() const;
    /** 双端只读开发资格，复用唯一项目开发策略；非发行、配置和命令行缺一拒绝，无副作用。 */
    static bool IsDevelopmentEnabledForCurrentProcess();
};

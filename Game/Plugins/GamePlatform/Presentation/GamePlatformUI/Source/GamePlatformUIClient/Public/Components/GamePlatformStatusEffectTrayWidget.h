#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "Engine/Texture2D.h"
#include "GamePlatformStatusEffectTrayWidget.generated.h"

/** EGamePlatformUIEffectPolarity（状态效果展示性质）。
 * 只描述可见效果的显示方向，不改变 GameplayEffect（玩法效果）的真实执行。
 */
UENUM(BlueprintType)
enum class EGamePlatformUIEffectPolarity : uint8
{
    Beneficial UMETA(DisplayName="增益"),
    Harmful UMETA(DisplayName="减益"),
    Neutral UMETA(DisplayName="中性"),
    Conditional UMETA(DisplayName="条件触发")
};

/** EGamePlatformUIEffectMechanic（状态效果主要机制）。
 * 一个效果可以附加多个VisibleMechanicTags（可见机制标签），本枚举仅用于高优先提示。
 */
UENUM(BlueprintType)
enum class EGamePlatformUIEffectMechanic : uint8
{
    None UMETA(DisplayName="无特殊机制"),
    Stun UMETA(DisplayName="眩晕"),
    Silence UMETA(DisplayName="沉默"),
    Root UMETA(DisplayName="定身"),
    Knockup UMETA(DisplayName="击飞"),
    Fear UMETA(DisplayName="恐惧"),
    Taunt UMETA(DisplayName="嘲讽"),
    Disarm UMETA(DisplayName="缴械"),
    PeriodicDamage UMETA(DisplayName="持续伤害"),
    PeriodicHealing UMETA(DisplayName="持续治疗"),
    Shield UMETA(DisplayName="护盾"),
    Immunity UMETA(DisplayName="免疫"),
    Proc UMETA(DisplayName="条件触发"),
    Encounter UMETA(DisplayName="关键战斗机制")
};

/** EGamePlatformUIEffectImportance（UI显示重要性；由领域客户端事实确定）。
 * 枚举次序用于一致性排序，不能由UI反推战斗伤害或权限。
 */
UENUM(BlueprintType)
enum class EGamePlatformUIEffectImportance : uint8
{
    Cosmetic UMETA(DisplayName="装饰"),
    LongTerm UMETA(DisplayName="长期状态"),
    Tactical UMETA(DisplayName="短期战术"),
    ActionCritical UMETA(DisplayName="操作关键"),
    HardControl UMETA(DisplayName="强控制"),
    CriticalMechanic UMETA(DisplayName="关键机制")
};

/** EGamePlatformUIDispelCategory（效果驱散展示类型）。
 * 仅作为已经授权的展示字段；是否能够驱散仍由游戏能力和服务器决定。
 */
UENUM(BlueprintType)
enum class EGamePlatformUIDispelCategory : uint8
{
    None UMETA(DisplayName="不可驱散或未知"),
    Magic UMETA(DisplayName="魔法"),
    Poison UMETA(DisplayName="中毒"),
    Curse UMETA(DisplayName="诅咒"),
    Disease UMETA(DisplayName="疾病"),
    Special UMETA(DisplayName="特殊")
};

/** FGamePlatformUIStatusEffect（状态效果图标展示快照）。
 * 保留旧EffectId/bBeneficial字段：未设置显式Polarity时继续按bBeneficial显示，
 * 避免破坏此前蓝图序列化和已存在的数据适配器。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIStatusEffect
{
    GENERATED_BODY()

    /** 效果类型语义ID；在旧接口中同时作为唯一身份，仍保持兼容。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    FName EffectId = NAME_None;

    /** 效果实例ID；同类型多施放者时必须提供，列表按此键精确去重。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    FName EffectInstanceId = NAME_None;

    /** 已获展示授权的施放者标识；未知或不可见时留空，禁止推断来源。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    FName SourceDisplayId = NAME_None;

    /** 本地化名称，绝不包含服务器内部对象指针或秘密属性。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    FText DisplayName;

    /** 图标软引用；资源加载失败时由视觉蓝图使用文字/形状回退。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    TSoftObjectPtr<UTexture2D> Icon;

    /** 叠层数，0层含义由玩法领域决定，UI不得擅自删除效果。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    int32 Stacks = 1;

    /** 旧版本有利/有害展示字段；只在bHasExplicitPolarity为false时使用。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    bool bBeneficial = true;

    /** 显式性质开关；默认false保持旧版本字段的表现行为。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    bool bHasExplicitPolarity = false;

    /** 展示性质，与控制机制正交。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    EGamePlatformUIEffectPolarity Polarity = EGamePlatformUIEffectPolarity::Beneficial;

    /** 主要控制/持续/免疫等可见机制，不影响实际GAS状态。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    EGamePlatformUIEffectMechanic PrimaryMechanic = EGamePlatformUIEffectMechanic::None;

    /** 额外的公开机制标签，不允许原样传入内部调试标签或私密效果标签。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    TArray<FName> VisibleMechanicTags;

    /** 显示重要性，从装饰到关键机制递增，UI不据此调整真实技能优先级。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    EGamePlatformUIEffectImportance Importance = EGamePlatformUIEffectImportance::Tactical;

    /** 可驱散类别；不能据此计算玩家是否有权执行驱散。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    EGamePlatformUIDispelCategory DispelCategory = EGamePlatformUIDispelCategory::None;

    /** 授权战斗适配器已确认当前玩家可以尝试驱散，仅影响按钮提示。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    bool bLocallyDispellable = false;

    /** 来自业务侧的关键机制显示事实，不从图标颜色或名字推断。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    bool bCriticalMechanic = false;

    /** 剩余秒数，-1表示永久；0表示已过期但尚未接收到业务移除事件。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    float RemainingSeconds = -1.0f;

    /** 是否含已校准的显示时间锚点；纯用于共享UI时钟校正倒计时。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    bool bHasExpirationTimeAnchor = false;

    /** 到期时间锚点，秒，按已校准来源的时间基准；不用于移除真实效果。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    double ExpirationTimeAnchorSeconds = 0.0;
};

/** FGamePlatformUIStatusEffectTrayState（单观察对象的一次完整授权快照）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIStatusEffectTrayState
{
    GENERATED_BODY()

    /** 显示对象身份；不可使用姓名代替稳定的真实作用域身份。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    FName OwnerDisplayId = NAME_None;

    /** 来源代次；显式绑定的客户端投影必须严格匹配，防止跨账号/世界串号。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    FGuid SourceScopeId;

    /** 严格递增修订号；更换作用域后从新投影版本开始。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    int64 Revision = -1;

    /** 最高64条已许可展示的效果；观察者权限过滤必须在进入UI前完成。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    TArray<FGamePlatformUIStatusEffect> Effects;
};

/** FGamePlatformUIStatusEffectDisplayPolicy（一个显示实例的视觉分组容量）。
 * 参数上限是显示容量，不是GameplayEffect实际数量或背包容量。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIStatusEffectDisplayPolicy
{
    GENERATED_BODY()

    /** 普通增益可见图标最大数，范围0～64。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    int32 MaxBeneficial = 12;

    /** 普通减益可见图标最大数，范围0～64。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    int32 MaxHarmful = 12;

    /** 紧急机制、重要控制可见图标最大数，范围1～64；超量需显示数量。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    int32 MaxCritical = 8;

    /** 中性和条件触发图标可见上限，范围0～64。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Effects")
    int32 MaxOther = 6;
};

/** FGamePlatformUIStatusEffectDisplayGroups（已排序/折叠的只读显示列表）。
 * 关键效果只进入Critical列表，避免同时出现在普通增/减益区造成重复。
 * 有超过上限的内容时必须展示OverflowCount（溢出数），不能悄悄忽略。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIStatusEffectDisplayGroups
{
    GENERATED_BODY()

    /** 可见增益图标列表。 */
    UPROPERTY(BlueprintReadOnly, Category="UI|Effects")
    TArray<FGamePlatformUIStatusEffect> Beneficial;

    /** 可见减益图标列表。 */
    UPROPERTY(BlueprintReadOnly, Category="UI|Effects")
    TArray<FGamePlatformUIStatusEffect> Harmful;

    /** 关键机制和强控制专用警报列表。 */
    UPROPERTY(BlueprintReadOnly, Category="UI|Effects")
    TArray<FGamePlatformUIStatusEffect> Critical;

    /** 中性/条件性效果列表。 */
    UPROPERTY(BlueprintReadOnly, Category="UI|Effects")
    TArray<FGamePlatformUIStatusEffect> Other;

    /** 未直接显示的普通增益数量（用于“+N”）。 */
    UPROPERTY(BlueprintReadOnly, Category="UI|Effects")
    int32 BeneficialOverflowCount = 0;

    /** 未直接显示的普通减益数量（用于“+N”）。 */
    UPROPERTY(BlueprintReadOnly, Category="UI|Effects")
    int32 HarmfulOverflowCount = 0;

    /** 被收纳的关键警报数量，必须有显式可见告警入口。 */
    UPROPERTY(BlueprintReadOnly, Category="UI|Effects")
    int32 CriticalOverflowCount = 0;

    /** 被收纳的其他效果数量。 */
    UPROPERTY(BlueprintReadOnly, Category="UI|Effects")
    int32 OtherOverflowCount = 0;
};

/** FGamePlatformUIStatusEffectPresentation（平台状态图标投影计算器）。
 * 无Widget/世界/网络依赖；适配器可独立验证快照并计算稳定显示顺序。
 */
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIStatusEffectPresentation
{
    /** 返回单效果稳定身份；未提供实例ID时回退旧EffectId（兼容旧调用）。 */
    static FName GetInstanceDisplayKey(const FGamePlatformUIStatusEffect& Effect);

    /** 返回兼容bBeneficial旧字段的实际展示性质。 */
    static EGamePlatformUIEffectPolarity GetDisplayPolarity(
        const FGamePlatformUIStatusEffect& Effect);

    /** 验证并按重要性/剩余时间/稳定身份排序，成功时替换OutGroups。 */
    static bool BuildDisplayGroups(
        const FGamePlatformUIStatusEffectTrayState& InState,
        const FGamePlatformUIStatusEffectDisplayPolicy& Policy,
        FGamePlatformUIStatusEffectDisplayGroups& OutGroups);
};

/** UGamePlatformStatusEffectTrayWidget（Buff/Debuff状态托盘）。
 * 平台只消费已授权只读快照，图标显示与分组由Blueprint实现；
 * 不读取ASC（能力系统）、不授权驱散、不使用Widget Tick。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformStatusEffectTrayWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()

public:
    /** 应用完整快照；参数无效、迟到或跨作用域时返回false，不改变当前画面。 */
    UFUNCTION(BlueprintCallable, Category="UI|Effects")
    bool ApplyEffects(const FGamePlatformUIStatusEffectTrayState& InState);

    /** 绑定当前授权的Owner+SourceScope；切换时清空旧图标和修订。 */
    UFUNCTION(BlueprintCallable, Category="UI|Effects")
    bool BindDisplayContext(FName InOwnerDisplayId, FGuid InSourceScopeId);

    /** 清除当前所有显示状态和来源，防止离开世界或换目标后残留图标。 */
    UFUNCTION(BlueprintCallable, Category="UI|Effects")
    void ClearEffects();

    /** 仅修改显示配额，经过重新投影后广播，不重复触发Gameplay事件。 */
    UFUNCTION(BlueprintCallable, Category="UI|Effects")
    bool ApplyDisplayPolicy(const FGamePlatformUIStatusEffectDisplayPolicy& InPolicy);

    /** 返回当前来源的未截断合法快照；对C++高频访问可用GetEffectsView。 */
    UFUNCTION(BlueprintPure, Category="UI|Effects")
    FGamePlatformUIStatusEffectTrayState GetEffects() const { return State; }

    /** 返回已排序分区（Buff、Debuff、Critical、Other）与显式溢出数量。 */
    UFUNCTION(BlueprintPure, Category="UI|Effects")
    FGamePlatformUIStatusEffectDisplayGroups GetDisplayGroups() const { return Groups; }

    /** C++零复制只读访问接口，不暴露可变状态。 */
    const FGamePlatformUIStatusEffectTrayState& GetEffectsView() const { return State; }
    const FGamePlatformUIStatusEffectDisplayGroups& GetDisplayGroupsView() const { return Groups; }

protected:
    /** 旧版事件契约，保持既有业务蓝图兼容。 */
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Effects",
        meta=(DisplayName="角色状态效果展示已变化"))
    void BP_OnEffectsChanged(FGamePlatformUIStatusEffectTrayState UpdatedState);

    /** 新分区事件：原子性完成分组与溢出后才通知蓝图。 */
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Effects",
        meta=(DisplayName="增益减益与控制效果分组已更新"))
    void BP_OnEffectDisplayGroupsChanged(
        FGamePlatformUIStatusEffectDisplayGroups UpdatedGroups);

private:
    /** 统一广播当前快照和分组；切换来源时广播空态避免画面残留。 */
    void BroadcastPresentation();

    /** 唯一绑定的来源身份；无绑定时只接受未携带FGuid的旧版投影。 */
    UPROPERTY(Transient)
    FGuid BoundSourceScopeId;

    /** 绑定的目标显示ID；必须和完整快照中的Owner一致。 */
    UPROPERTY(Transient)
    FName BoundOwnerDisplayId = NAME_None;

    /** 当前被平台接受的业务可见状态，不能作为Gameplay权威状态。 */
    UPROPERTY(Transient)
    FGamePlatformUIStatusEffectTrayState State;

    /** 缓存已完成排序/截断的只读显示结果，避免Widget每帧重复排序。 */
    UPROPERTY(Transient)
    FGamePlatformUIStatusEffectDisplayGroups Groups;

    /** 配额可按角色、设备和玩家HUD预设调整，但不允许删除权威效果。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Effects",
        meta=(AllowPrivateAccess="true"))
    FGamePlatformUIStatusEffectDisplayPolicy DisplayPolicy;
};


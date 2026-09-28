#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Requests/GamePlatformUIFeedbackRequest.h"
#include "DivineBeastsCombatUIFeedbackLibrary.generated.h"

class UGamePlatformFeedbackWidget;
class ULocalPlayer;

/** EDivineBeastsCombatFeedbackKind（神兽联盟客户端战斗反馈类型）。 */
UENUM(BlueprintType)
enum class EDivineBeastsCombatFeedbackKind : uint8
{
    Damage,
    ShieldDamage,
    Healing,
    Death
};

/**
 * FDivineBeastsCombatFeedbackInput（神兽联盟客户端战斗反馈输入DTO）。
 *
 * 这是UI边界只读数据，不是Gameplay（玩法）权威状态。
 * 上游Gameplay/Presentation Adapter（玩法/表现适配器）必须在战斗事实已经安全到达客户端后再构造本结构。
 * 这样DivineBeastsUIClient（神兽联盟UI客户端模块）不直接依赖尚未稳定的GamePlatformCombat（平台战斗模块）。
 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsCombatFeedbackInput
{
    GENERATED_BODY()

    /** 唯一事实ID，用于预测确认/重复传输视觉去重。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    FGuid EventId;

    /** 当前反馈类型。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    EDivineBeastsCombatFeedbackKind Kind =
        EDivineBeastsCombatFeedbackKind::Damage;

    /** 已经由上游确认的展示数值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    double Magnitude = 0.0;

    /** 世界反馈锚点，通常为命中点或目标头顶位置。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    FVector WorldLocation = FVector::ZeroVector;

    /** 稳定目标视觉键，用于同目标短时间数值合并；不要求是后端主键。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    FName TargetVisualKey = NAME_None;
};

/**
 * UDivineBeastsCombatUIFeedbackLibrary（神兽联盟战斗UI反馈映射库）。
 *
 * 边界：
 * - 输入必须是“已经到达当前客户端”的只读战斗反馈DTO（数据传输对象）。
 * - 本模块不依赖GamePlatformCombat，不把普通本地委托误当网络复制机制。
 * - 服务器事实应由Gameplay/Presentation Adapter通过正式复制/GAS表现事件转换后再调用本映射。
 *
 * 本类只把项目战斗语义映射为平台中立 FeedbackRequest（反馈请求），
 * 具体字体、颜色、动画由 DBA UI Definition / Widget Blueprint 决定。
 */
UCLASS()
class DIVINEBEASTSUICLIENT_API UDivineBeastsCombatUIFeedbackLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * 把客户端可见战斗反馈DTO转换为伤害/治疗/死亡等浮动反馈请求。
     * 本函数不读取Actor、ASC（能力系统组件）或服务器状态。
     */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Combat")
    static bool BuildFloatingTextRequest(
        const FDivineBeastsCombatFeedbackInput& Input,
        FGamePlatformUIFeedbackRequest& OutRequest);


    /**
     * 将一条已经到达指定LocalPlayer（本地玩家）的战斗反馈DTO提交到平台FeedbackService。
     *
     * @param LocalPlayer 明确的本地玩家作用域；禁止通过全局第0号玩家猜测上下文。
     * @param Input 已经在客户端可见的战斗反馈DTO。
     * @param WidgetClass 由项目UI内容定义提供的浮动反馈Widget类。
     * @return 成功时返回反馈OccurrenceId；失败时返回无效Guid。
     */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Combat")
    static FGuid SubmitFloatingText(
        ULocalPlayer* LocalPlayer,
        const FDivineBeastsCombatFeedbackInput& Input,
        TSubclassOf<UGamePlatformFeedbackWidget> WidgetClass);
};

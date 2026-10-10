// 平台客户端UI主题公开值契约；不引用游戏资源、网络状态或全局可变样式。
#pragma once
#include "CoreMinimal.h"
#include "GamePlatformUIThemeTypes.generated.h"

/** 当前第一批统一样式种类；对应CommonUI原生样式类。 */
UENUM(BlueprintType)
enum class EGamePlatformUIStyleKind : uint8 { Button, Text, Border };
/** P13来源排序；先资格/精确语义，再按此等级和明确条件排序。 */
UENUM(BlueprintType)
enum class EGamePlatformUIStyleScope : uint8 { Platform, Moba, Project, ContentPack };

/** 本地玩家上下文；空身份或画质-1表示未指定，不匹配相应受限资源。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIThemeContext
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Theme") FName PlatformId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Theme") FName SkinId = NAME_None;
    /** 0..4为具体档位；-1为未指定。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Theme") int32 QualityTier = -1;
    bool IsValid() const { return QualityTier >= -1 && QualityTier <= 4; }
};

/** 单条样式选择规则：只允许显式等值约束计入具体度，不依赖数组顺序。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIStyleRule
{
    GENERATED_BODY()
    /** UI.Style.*中立语义，不是图片路径或项目身份。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") FName StyleId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") EGamePlatformUIStyleScope Scope = EGamePlatformUIStyleScope::Platform;
    /** 空约束为不限制，非空必须与当前上下文相等。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") FName PlatformId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") FName SkinId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") int32 QualityTier = -1;
    /** 同scope与具体度下的优先级，范围-1000..1000。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") int32 Priority = 0;
    bool Matches(const FGamePlatformUIThemeContext& Context) const
    {
        return (PlatformId.IsNone() || PlatformId == Context.PlatformId) &&
            (SkinId.IsNone() || SkinId == Context.SkinId) &&
            (QualityTier < 0 || QualityTier == Context.QualityTier);
    }
    int32 Specificity() const
    { return int32(!PlatformId.IsNone()) + int32(!SkinId.IsNone()) + int32(QualityTier >= 0); }
};

/** 必需语义，任何一项在当前上下文缺失或歧义均拒绝切换。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIStyleRequirement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") EGamePlatformUIStyleKind Kind = EGamePlatformUIStyleKind::Button;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") FName StyleId = NAME_None;
};

/** 命名控件显式绑定；默认空数组保持历史蓝图不变，不扫描其他玩家或跨UserWidget树。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIWidgetStyleBinding
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") FName WidgetName = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") FName StyleId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") EGamePlatformUIStyleKind Kind = EGamePlatformUIStyleKind::Button;
    /** 显式允许后才尝试父语义。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") bool bAllowParentFallback = false;
    /** 替换字体时保留已经人工验收的字号。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") bool bPreserveFontSize = true;
    /** 可选装饰可跳过；关键按钮/文本默认必需。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") bool bOptional = false;
};

// 平台主题定义：沿用统一LogicalId/DataVersion，只持有CommonUI样式软类。
#pragma once
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Styling/GamePlatformUIThemeTypes.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "CommonBorder.h"
#include "GamePlatformUIThemeDefinition.generated.h"

/** 类型明确的按钮条目，UI资源分组由GamePlatformData加载。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIButtonThemeEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") FGamePlatformUIStyleRule Rule;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme", meta=(AssetBundles="UI")) TSoftClassPtr<UCommonButtonStyle> StyleClass;
};
/** 类型明确的文字条目。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUITextThemeEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") FGamePlatformUIStyleRule Rule;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme", meta=(AssetBundles="UI")) TSoftClassPtr<UCommonTextStyle> StyleClass;
};
/** 类型明确的边框/背景条目，九宫格等数据由原生BorderStyle保存。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIBorderThemeEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme") FGamePlatformUIStyleRule Rule;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Theme", meta=(AssetBundles="UI")) TSoftClassPtr<UCommonBorderStyle> StyleClass;
};

/** 项目只创建数据实例，纯外观差异不需要额外项目子类。运行时不修改此定义或样式CDO。 */
UCLASS(BlueprintType)
class GAMEPLATFORMUICLIENT_API UGamePlatformUIThemeDefinition : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Theme") TArray<FGamePlatformUIButtonThemeEntry> Buttons;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Theme") TArray<FGamePlatformUITextThemeEntry> Texts;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Theme") TArray<FGamePlatformUIBorderThemeEntry> Borders;
    /** 必需语义必须精确满足；不会暗中使用父样式。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Theme") TArray<FGamePlatformUIStyleRequirement> RequiredStyles;
    virtual FGamePlatformResult ValidateDefinition() const override;
    /** 主题是纯客户端内容；编辑器可读取供Cook审计，但专用服务器不需要该资产。 */
    virtual bool NeedsLoadForServer() const override { return false; }
    /** 唯一条目索引；缺失/歧义返回INDEX_NONE，Error明确原因，不进行资源加载。 */
    int32 ResolveButtonStyle(FName Key, const FGamePlatformUIThemeContext& Context, bool bAllowParentFallback, FText& Error) const;
    int32 ResolveTextStyle(FName Key, const FGamePlatformUIThemeContext& Context, bool bAllowParentFallback, FText& Error) const;
    int32 ResolveBorderStyle(FName Key, const FGamePlatformUIThemeContext& Context, bool bAllowParentFallback, FText& Error) const;
    /** 原子切换前检查类型、完整加载与必需语义。 */
    bool ValidateLoadedStyles(const FGamePlatformUIThemeContext& Context, FText& Error) const;
    /** 只读解析已加载样式类；失败返回空，不在绘制路径触发加载。 */
    UClass* ResolveLoadedStyle(EGamePlatformUIStyleKind Kind, FName Key,
        const FGamePlatformUIThemeContext& Context, bool bAllowParentFallback, FText& Error) const;
};

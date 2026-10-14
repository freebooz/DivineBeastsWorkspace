// 第三层前端水体方案选择：完全独立于Dedicated Server、正式Village Water和Gameplay。
// 原生已有Sanctuary水材质/水幕为唯一默认选择，FluidFlux为未安装时自动安全回退的可选特写。
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DivineBeastsCharacterStudioWaterPolicy.generated.h"

/** 角色工作室水效选择结果：只是合资格判断，不代表插件已启动或资产已加载。 */
UENUM(BlueprintType)
enum class EDivineBeastsCharacterStudioWaterMode : uint8
{
    Native UMETA(DisplayName="原生水材质（默认）"),
    FluidFluxEligible UMETA(DisplayName="Fluid Flux可选特写（需实际插件/资产）")
};

/**
 * UDivineBeastsCharacterStudioWaterPolicy（角色预览流体选型）。
 * 不包含任何第三方头文件、FluidFlux UObject引用、服务器网络能力或关卡硬引用。
 */
UCLASS()
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsCharacterStudioWaterPolicy
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 对角色选择工作室当前舞台判断是否允许可选流体方案；
     * bPreferFluidFlux需由显式用户/项目配置提出，bHighQuality为客户端高画质许可。
     * 仅独立前端预览世界，非客户端或插件缺失时返回Native。
     * 即使返回Eligible也不生成第三方Actor，需经单独正式适配插件加载并完成功能验证。
     */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|CharacterPreview|Water")
    static EDivineBeastsCharacterStudioWaterMode ResolveWaterMode(
        const AActor* PreviewStage, bool bPreferFluidFlux, bool bHighQuality);

    /** 纯能力检查，用于UI禁用未安装插件的开关；不触发插件加载和HTTP请求。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|CharacterPreview|Water")
    static bool IsFluidFluxPluginEligible();
};

#pragma once

// DivineBeasts（项目层）仅客户端开发模式契约。
// 不复制平台ApplicationFlow，不存放认证状态；Shipping及服务器永远按正式Debug入口执行。
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DivineBeastsDevelopmentLaunchMode.generated.h"

/** 开发启动分支：Debug保留真实登录，PluginDev只允许本地非发行客户端使用。 */
UENUM(BlueprintType)
enum class EDBADevelopmentLaunchMode : uint8
{
    Debug UMETA(DisplayName="完整流程调试"),
    PluginDev UMETA(DisplayName="插件开发")
};

/** 兼容开发模式枚举的只读适配层：真正权限始终委托FDivineBeastsPluginDemoMode（唯一门禁）。 */
struct FDBADevelopmentLaunchMode
{
    /** 模拟唯一许可规则，需成对的-DBAPluginDemo与-DBAPluginDemoMap，不能再接受另一套独立开关。 */
    static EDBADevelopmentLaunchMode Resolve(
        const TCHAR* CommandLine, bool bIsShipping, bool bDedicatedServer, bool bCommandlet);

    /** 当前进程模式；只在启动分流处读取，不逐帧扫描、不存账号或世界状态。 */
    static EDBADevelopmentLaunchMode ResolveCurrent();
};

/** 仅暴露只读模式给Monolith制作的项目Portal Widget，不提供绕过登录的业务命令。 */
UCLASS()
class DIVINEBEASTSUICLIENT_API UDBADevelopmentLaunchModeLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** 是否为显式授权的本地插件开发模式；正式发行版、服务器必定为false。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Development")
    static bool IsPluginDevelopmentMode();
};

#pragma once
#include "Types/GamePlatformResult.h"
class UPCGComponent;
class UGamePlatformPCGProfileDefinition;

/** 原生组件适配器及编辑器工具共同使用的只读审查结果，不公开受管资源容器。 */
struct FGamePlatformPCGOutputInspection
{
    int32 InstanceCount = 0;
    FString Fingerprint;
    FGamePlatformResult Result;
};

/** 限定创作/集成API，游戏线程；调用者必须独占未运行的原生组件，不传其他系统正在使用的组件。 */
namespace GamePlatformPCGInspection
{
    /** 审查完整有限图形状与所有输出属性；子图/蓝图/未知节点/GPU/参数旁路拒绝，不修改原图。 */
    GAMEPLATFORMPCG_API FGamePlatformResult ValidateApprovedGraph(const UGamePlatformPCGProfileDefinition& Profile);
    /** 克隆为组件独占的瞬态图、写入已验证参数；不调用Generate、不改变共享源图或返回可变图。 */
    GAMEPLATFORMPCG_API FGamePlatformResult ConfigureOwnedComponent(UPCGComponent& Component,
        const UGamePlatformPCGProfileDefinition& Profile,int32 DerivedSeed);
    /** 仅遍历此组件受管ISM，按实例数计数并核对用途/边界；没有PCG完成委托时不得据此宣称生成终态。 */
    GAMEPLATFORMPCG_API FGamePlatformPCGOutputInspection InspectOwnedOutput(UPCGComponent& Component,
        const UGamePlatformPCGProfileDefinition& Profile,const FVector& CenterCm);
}

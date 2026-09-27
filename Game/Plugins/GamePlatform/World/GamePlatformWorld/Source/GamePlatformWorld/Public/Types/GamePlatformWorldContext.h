#pragma once
#include "CoreMinimal.h"
#include "Types/GamePlatformId.h"
#include "Types/GamePlatformVersion.h"
#include "Types/GamePlatformResult.h"

/** 未初始化、等待、就绪、失败、失效；Ready可因Provider退出或世界销毁而失效。 */
enum class EGamePlatformWorldReadiness : uint8 { Uninitialized, Waiting, Ready, Failed, Invalidated };
/** 开发上下文不是生产服务器注册；网络投影必须来自已验证Session，当前前置未具备。 */
enum class EGamePlatformWorldAuthority : uint8 { Unbound, DevelopmentLocal, DevelopmentServer, SessionProjection };

/** 游戏线程复制的值快照；不含票据、对象指针或可写实例分配。 */
struct FGamePlatformWorldContext
{
    /** 来自真实WorldDefinition.LogicalId，稳定于地图文件重命名。 */
    FGamePlatformId WorldId;
    /** 来自WorldDefinition的可选默认体验；空值不表示默认正式业务。 */
    FGamePlatformId ExperienceId;
    /** 仅受控专用服务器开发参数提供；非分片调度结果。 */
    FName ServerRole;
    /** Session缺失时保持空，不从客户端GUID合成服务器身份。 */
    FString ServerInstanceId;
    /** 控制面尚未绑定，0表示未提供，不伪造启动代次。 */
    uint64 ServerStartGeneration = 0;
    /** 可选权威分片投影；本地开发恒为空。 */
    FString ShardId;
    /** 明确观察主体的当前区域；多观察者时使用观察者查询，不能当全体玩家位置。 */
    FGamePlatformId RegionId;
    /** 由显式启动装配传入并验证，非引擎版本或数据结构版本。 */
    FGamePlatformVersion BuildVersion;
    /** 当前UWorld实际包名，移除PIE前缀，不由调用者替换。 */
    FString MapPackageName;
    /** 每个UWorld创建独立GUID；旧世界句柄不能操作新世界。 */
    FGuid ContextGeneration;
    /** 明示来源可信边界；Development不等于生产Authority。 */
    EGamePlatformWorldAuthority AuthorityKind = EGamePlatformWorldAuthority::Unbound;
    /** 当前事实聚合，不根据地图存在或固定延时返回Ready。 */
    EGamePlatformWorldReadiness ReadinessState = EGamePlatformWorldReadiness::Uninitialized;
    /** 失败码与中文说明；默认未执行。 */
    FGamePlatformResult Result;
};

/** 每项独立记录，游戏线程值读取；额外贡献者不能覆盖基础条件。 */
struct FGamePlatformWorldReadinessSnapshot
{
    bool bWorldObjectValid = false; // 当前运行世界且已BeginPlay。
    bool bDefinitionLoaded = false; // 真实Data租约可读。
    bool bMapIdentityMatched = false; // 定义软引用包与真实包相同。
    bool bSessionContextMatched = false; // 真Session匹配或显式离线开发例外。
    bool bRequiredRegionsRegistered = false; // 必需定义对应的弱Provider均活着。
    bool bRequiredStreamingReady = false; // 必需流送请求的真实底层状态。
    bool bWorldNotTearingDown = false; // 未进入关闭。
    bool bContributorsReady = false; // 所有已登记扩展贡献者成功。
    FGamePlatformWorldContext Context; // 同一次采样的只读上下文。
};

#pragma once

#include "CoreMinimal.h"

class UObject;
class UWorld;

/** 请求所属生命周期；跨地图身份留在实例，不因世界请求取消而自动退出。 */
enum class EGamePlatformOnlineRequestLifetime : uint8
{
    /** 允许跨地图执行，仍受可选调用者和游戏实例销毁约束。 */
    GameInstance,
    /** 必须提供本实例的 World；该世界清理时取消请求。 */
    World
};

/** 请求作用域与时间选择；所有入口必须在游戏线程调用。 */
struct FGamePlatformOnlineRequestOptions
{
    /** 默认实例作用域；世界生命周期必须显式选择。 */
    EGamePlatformOnlineRequestLifetime Lifetime = EGamePlatformOnlineRequestLifetime::GameInstance;
    /** 可选弱调用者，不延长 UObject 寿命；曾有效的调用者失效后取消请求。 */
    TWeakObjectPtr<UObject> Owner;
    /** 世界生命周期时必填，必须属于取得服务的游戏实例；不通过 GWorld 推导。 */
    TWeakObjectPtr<UWorld> World;
    /** 0 使用实例配置，否则须为正且不超过配置总上限；包含刷新与排队时间。 */
    double DeadlineSeconds = 0.0;
};

/** 本实例生成的逻辑请求身份；字段不得人工拼装，旧实例句柄不能取消新实例请求。 */
struct FGamePlatformOnlineRequestHandle
{
    /** 随游戏实例服务创建的随机作用域，不是玩家或服务端身份。 */
    FGuid InstanceScopeId;
    /** 每次公开异步调用独立生成；完成后不可重复使用。 */
    FGuid RequestId;
};

/** 密码登录输入，只存在调用方与请求的临时内存；不支持游客降级、记住登录或分屏多账号。 */
struct FGamePlatformOnlineLoginRequest
{
    /** 正常身份链校验的账号名；不得打印完整认证输入。 */
    FString AccountName;
    /** 密码原文，仅作为输入；禁止写入日志、命令行、配置、资产、诊断及存档。字符串析构不是安全擦除证明。 */
    FString Credential;
    /** 可选设备或安装身份，不是设备密钥；空值表示未提供。 */
    FString DeviceId;
};

/** 唯一开放的公共资料更新白名单；身份由后端认证解析，不接受目标 PlayerId。 */
struct FGamePlatformOnlineProfileUpdateRequest
{
    /** 去除首尾空白后为 1～24 个 Unicode 字符；不能借此修改权益、余额或隐藏分。 */
    FString DisplayName;
    /** 已读取资料的非负修订号；-1 默认非法，禁止以默认值执行盲覆盖。 */
    int64 ExpectedRevision = -1;
    /** 调用方为同一逻辑更新生成的非空幂等键，最多 128 字符；重试必须保留原键、原内容与原修订号。 */
    FString IdempotencyKey;
};

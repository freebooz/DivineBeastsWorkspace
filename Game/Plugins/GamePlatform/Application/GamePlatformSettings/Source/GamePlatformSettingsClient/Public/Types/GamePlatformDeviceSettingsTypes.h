#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformResult.h"
#include "GamePlatformDeviceSettingsTypes.generated.h"

/** 平台稳定窗口模式；避免把引擎内部枚举直接扩散到上层设置界面。 */
UENUM(BlueprintType)
enum class EGamePlatformWindowMode : uint8
{
    Windowed,
    WindowedFullscreen,
    Fullscreen
};

/** 设置应用方式：预览不会写盘，提交会确认显示模式并持久化。 */
UENUM(BlueprintType)
enum class EGamePlatformDeviceSettingsApplyMode : uint8
{
    Preview,
    Commit
};

/**
 * 设备／应用级客户端设置。
 *
 * 只承载由 UGameUserSettings（引擎用户设置）拥有的跨游戏设备设置。
 * 输入重绑、输入舒适度、UI 可访问性、相机手感和音频播放参数继续由各领域插件拥有，
 * GamePlatformSettings 不复制这些领域模型。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSETTINGSCLIENT_API FGamePlatformDeviceSettings
{
    GENERATED_BODY()

    /** 输出分辨率；提交前必须通过平台安全校验。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Display")
    FIntPoint Resolution = FIntPoint::ZeroValue;

    /** 窗口／全屏模式。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Display")
    EGamePlatformWindowMode WindowMode = EGamePlatformWindowMode::WindowedFullscreen;

    /** 是否启用垂直同步。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Display")
    bool bVSyncEnabled = false;

    /** 帧率上限；0 表示不限制。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Performance")
    float FrameRateLimit = 0.0f;

    /** 以下画质等级均使用 UE 原生 0..4 语义。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Quality")
    int32 ViewDistanceQuality = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Quality")
    int32 AntiAliasingQuality = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Quality")
    int32 ShadowQuality = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Quality")
    int32 GlobalIlluminationQuality = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Quality")
    int32 ReflectionQuality = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Quality")
    int32 PostProcessQuality = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Quality")
    int32 TextureQuality = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Quality")
    int32 EffectsQuality = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Quality")
    int32 FoliageQuality = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Quality")
    int32 ShadingQuality = 3;
};

/** 设置服务只读快照；UI 应事件驱动消费，不应逐帧轮询。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSETTINGSCLIENT_API FGamePlatformDeviceSettingsSnapshot
{
    GENERATED_BODY()

    /** 当前由 UGameUserSettings 持有并已应用到运行时的值。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    FGamePlatformDeviceSettings Current;

    /** 尚未应用或等待预览／提交的候选值。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    FGamePlatformDeviceSettings Staged;

    /** Staged 与 Current 是否存在语义差异。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    bool bHasStagedChanges = false;

    /** 当前显示／画质设置是否处于未确认预览状态。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    bool bPreviewActive = false;

    /** 公开快照真实变化时递增，供 UI 去重。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    int64 Revision = 0;
};

/** 轻量运行诊断；不包含账号、输入内容、硬件唯一标识或其他敏感信息。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSETTINGSCLIENT_API FGamePlatformDeviceSettingsDiagnostics
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int32 SubscriptionCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int64 StageCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int64 ApplyCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int64 SaveCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int64 PreviewCancelCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int64 RejectedMutationCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int64 SubscriberCallbackCount = 0;
};

/** 设置状态订阅句柄；作用域和代次共同阻止跨 GameInstance 或旧实例误操作。 */
struct FGamePlatformDeviceSettingsSubscription
{
    FGuid ScopeId;
    FGuid Id;
    uint64 Generation = 0;

    bool IsValid() const
    {
        return ScopeId.IsValid() && Id.IsValid() && Generation != 0;
    }
};

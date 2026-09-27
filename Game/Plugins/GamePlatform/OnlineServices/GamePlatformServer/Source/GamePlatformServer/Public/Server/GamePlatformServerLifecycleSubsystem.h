#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GamePlatformServerLifecycleSubsystem.generated.h"

/**
 * EGamePlatformServerLifecycleState（游戏平台服务器生命周期状态）。
 * Ready只表示控制面确认实例可接纳；Drain只表示排空已启动，不代替本地玩家清理。
 */
UENUM(BlueprintType)
enum class EGamePlatformServerLifecycleState : uint8
{
    Unregistered,
    Registering,
    Registered,
    PublishingReady,
    Ready,
    Draining,
    Stopped,
    Failed
};

/**
 * FGamePlatformServerInstanceInfo（游戏平台服务器实例注册信息）。
 * 由服务器组合根在Profile与环境身份校验后显式提交；不含认证凭据。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSERVER_API FGamePlatformServerInstanceInfo
{
    GENERATED_BODY()

    /** Shared契约中的游戏身份；不得用本地显示名称替代。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Server")
    FString GameId;

    /** 控制面分配或部署注入的唯一服务器实例身份。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Server")
    FString GameServerId;

    /** 中立服务器角色标识；具体允许值由游戏项目契约校验。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Server")
    FString ServerRoleId;

    /** 本实例启动承载的项目体验身份；须与ServerRoleId匹配。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Server")
    FString ExperienceId;

    /** 逻辑世界或分片身份，不是地图文件路径。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Server")
    FString WorldId;

    /** 部署区域身份，用于匹配和调度。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Server")
    FString RegionId;

    /** 可选Kubernetes或Agones集群身份；无容器编排时允许为空。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Server")
    FString ClusterId;

    /** 可选承载节点身份；由部署系统注入，不由客户端提供。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Server")
    FString NodeId;

    /** 服务器制品版本；必须由构建／部署系统提供。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Server")
    FString BuildVersion;

    /** UE实时网络协议版本；零表示部署尚未提供版本标记，后端按其策略处理。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Server")
    int32 ProtocolVersion = 0;

    /** 客户端连接端点；禁止携带凭据或换行控制字符。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Server")
    FString PublicEndpoint;

    /** 后端可分配的最大玩家数；心跳人数不得为负或超过此值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Server")
    int32 Capacity = 0;

    /** 调用前检查完整非秘密身份、正容量和无换行端点；不检查凭据或连接后端。 */
    bool IsValid() const;
};

/** FGamePlatformServerControlCompletion（游戏平台服务器控制面操作完成回调）。 */
using FGamePlatformServerControlCompletion = TFunction<void(bool, FName)>;

/**
 * IGamePlatformServerControlProvider（游戏平台服务器控制面提供者）。
 * HTTP/gRPC适配器在独立实现模块注册；调用发生于游戏线程，完成回调允许来自任意线程。
 * 提供者必须保证每次回调至多一次，错误码不得包含凭据、端点或玩家个人信息。
 */
class GAMEPLATFORMSERVER_API IGamePlatformServerControlProvider
{
public:
    virtual ~IGamePlatformServerControlProvider() = default;

    static FName GetModularFeatureName();

    /** 注册实例；身份冲突或校验错误通过脱敏错误码完成，不返回凭据。 */
    virtual void RegisterInstance(
        const FGamePlatformServerInstanceInfo& Instance,
        FGamePlatformServerControlCompletion Completion) = 0;
    /** 汇报当前人数和Starting/Ready/Draining状态；调用者负责提供真实人数。 */
    virtual void SendHeartbeat(
        const FGamePlatformServerInstanceInfo& Instance,
        int32 CurrentPlayers,
        const FString& Status,
        FGamePlatformServerControlCompletion Completion) = 0;
    /** 只有项目资源和世界门禁通过后才能调用；后端接受前实例不可分配。 */
    virtual void PublishReady(
        const FGamePlatformServerInstanceInfo& Instance,
        FGamePlatformServerControlCompletion Completion) = 0;
    /** 停止接受新分配；是否已清空本地玩家由调用方另行判定。 */
    virtual void BeginDrain(
        const FGamePlatformServerInstanceInfo& Instance,
        FGamePlatformServerControlCompletion Completion) = 0;
};

/**
 * FGamePlatformServerLifecycleSnapshot（游戏平台服务器生命周期快照）。
 * 不暴露Endpoint；Generation用于诊断，不能代替内部异步代次校验。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSERVER_API FGamePlatformServerLifecycleSnapshot
{
    GENERATED_BODY()

    /** 本地与控制面确认后的生命周期阶段。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Server")
    EGamePlatformServerLifecycleState State = EGamePlatformServerLifecycleState::Unregistered;

    /** 已注册服务器身份；未注册时为空。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Server")
    FString GameServerId;

    /** Profile校验通过的服务器角色身份。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Server")
    FString ServerRoleId;

    /** 当前激活的体验身份。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Server")
    FString ExperienceId;

    /** 脱敏失败码；不会包含URL、令牌或玩家数据。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Server")
    FName ErrorCode = NAME_None;

    /**
     * 仅供C++诊断状态推进，不构成可调用的操作句柄。
     * UE反射不支持uint64蓝图属性，因此该内部代次保持非UPROPERTY，避免改变原生比较与过期回调判定语义。
     */
    uint64 OperationGeneration = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformServerLifecycleChangedNative,
    const FGamePlatformServerLifecycleSnapshot&);

/**
 * UGamePlatformServerLifecycleSubsystem（游戏平台服务器生命周期子系统）。
 * 按GameInstance隔离；Initialize不连接控制面，只有组合根显式调用RegisterInstance后才联网。
 */
UCLASS()
class GAMEPLATFORMSERVER_API UGamePlatformServerLifecycleSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Deinitialize() override;

    /** 唯一提供者存在且实例信息完整时开始注册；重复注册、缺失或多提供者均失败关闭。 */
    bool RegisterInstance(const FGamePlatformServerInstanceInfo& Instance);
    /** 仅Registered/Ready/Draining实例可上报心跳；失败转Failed，禁止继续宣称Ready。 */
    bool SendHeartbeat(int32 CurrentPlayers);
    /** 只有项目资源与世界门禁完成后才能显式发布Ready。 */
    bool MarkReady();
    /** 在停止接纳新会话前通知控制面开始Drain；不自动销毁World或踢出玩家。 */
    bool BeginDrain();
    /** 本地玩家／比赛资源已清理后结束本地生命周期；控制面Drain由BeginDrain完成。 */
    bool CompleteDrain();

    FGamePlatformServerLifecycleSnapshot GetSnapshot() const { return Snapshot; }
    FGamePlatformServerLifecycleChangedNative& OnLifecycleChanged()
    {
        return LifecycleChanged;
    }

private:
    enum class EControlOperation : uint8
    {
        Register,
        Heartbeat,
        Ready,
        BeginDrain
    };

    IGamePlatformServerControlProvider* ResolveUniqueProvider();
    void CompleteOperation(
        uint64 Generation,
        EControlOperation Operation,
        bool bSucceeded,
        FName ErrorCode);
    void SetState(EGamePlatformServerLifecycleState State, FName ErrorCode = NAME_None);
    bool StartProviderOperation(
        EControlOperation Operation,
        int32 CurrentPlayers = 0);

    FGamePlatformServerInstanceInfo ActiveInstance;
    FGamePlatformServerLifecycleSnapshot Snapshot;
    FGamePlatformServerLifecycleChangedNative LifecycleChanged;
    uint64 Generation = 0;
    bool bHeartbeatInFlight = false;
    bool bControlOperationInFlight = false;
};

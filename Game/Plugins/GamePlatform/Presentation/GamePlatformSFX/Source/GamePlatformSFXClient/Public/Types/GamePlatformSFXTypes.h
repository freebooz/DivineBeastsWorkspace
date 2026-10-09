#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GamePlatformSFXTypes.generated.h"

class USceneComponent;
class UWorld;

/** EGamePlatformSFXPlaybackSpace（音效播放空间）。 */
UENUM(BlueprintType)
enum class EGamePlatformSFXPlaybackSpace : uint8
{
    /** 非空间化2D音效，适用于UI与本地反馈；仍绑定当前World生命周期。 */
    TwoD,
    /** 在世界坐标播放的3D音效。 */
    World,
    /** 附着到场景组件的3D音效；Owner销毁策略由Definition控制。 */
    Attached
};

/** 客户端表现请求的预测终态；只控制本世界去重，没有网络权威含义。 */
UENUM(BlueprintType)
enum class EGamePlatformSFXPredictionState : uint8 { None, Predicted, Confirmed, Corrected, Cancelled };

/** EGamePlatformSFXResultCode（音效请求结果码）。 */
UENUM(BlueprintType)
enum class EGamePlatformSFXResultCode : uint8
{
    Played,
    Queued,
    Cancelled,
    InvalidRequest,
    InvalidWorld,
    UnsupportedEnvironment,
    DataUnavailable,
    LoadRejected,
    LoadFailed,
    DefinitionInvalid,
    AssetUnavailable,
    OwnerInvalid,
    SpawnFailed,
    StaleRequest,
    /** 已完成的请求被确认；未创建新的音频组件，Handle为空。 */
    AlreadyCompleted
};

/**
 * FGamePlatformSFXHandle（音效实例句柄）。
 * Id标识一次播放事务；Generation与弱World身份共同阻止跨世界、旧代次误操作。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSFXCLIENT_API FGamePlatformSFXHandle
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX")
    FGuid Id;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX")
    int32 Generation = 0;

    /** 弱世界引用不延长World生命周期。 */
    UPROPERTY(Transient)
    TWeakObjectPtr<UWorld> World;

    bool IsValid() const { return Id.IsValid() && Generation > 0 && World.IsValid(); }
    bool BelongsToWorld(const UWorld* InWorld) const { return World.Get() == InWorld; }
};

/**
 * FGamePlatformSFXRequest（平台音效播放请求）。
 * DefinitionId必须是GamePlatformData逻辑身份规范字符串，不是资产路径。
 * AttachComponent仅供C++附着播放使用，弱引用避免异步Definition加载期间延长Owner生命周期。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSFXCLIENT_API FGamePlatformSFXRequest
{
    GENERATED_BODY()

    /** 跨Presentation/SFX链路的请求身份；有效时用于预测请求去重与取消。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX")
    FGuid RequestId;

    /** 同RequestId预测/确认共享身份；Corrected可替换正常完成实例，Cancelled在历史保留期不可复活。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX")
    EGamePlatformSFXPredictionState PredictionState = EGamePlatformSFXPredictionState::None;

    /** 例如 presentation.sfx.hit@1；不得填写 /Game/... 资产路径。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX")
    FName DefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX")
    FName ContextId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX")
    FGameplayTagContainer ContextTags;

    /** World播放位置；Attached模式由Definition决定使用AttachComponent。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX")
    FRotator Rotation = FRotator::ZeroRotator;

    /** 请求级线性音量倍数；与Definition默认值相乘。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX", meta=(ClampMin="0.0", ClampMax="4.0"))
    float VolumeMultiplier = 1.0f;

    /** 请求级音高倍数；与Definition默认值相乘。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX", meta=(ClampMin="0.25", ClampMax="4.0"))
    float PitchMultiplier = 1.0f;

    /** 从声音内部开始播放的秒数。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX", meta=(ClampMin="0.0"))
    float StartTimeSeconds = 0.0f;

    /** 只允许Definition白名单声明的浮点参数，适用于MetaSound/SoundCue参数。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX")
    TMap<FName, float> FloatParameters;

    /** 附着点名；仅Attached模式使用。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX")
    FName AttachPointName = NAME_None;

    /** 弱附着目标；不参与反射序列化，也不能作为跨网络权威引用。 */
    TWeakObjectPtr<USceneComponent> AttachComponent;
};

/** FGamePlatformSFXResult（音效播放受理结果）。Queued表示Definition异步租约已受理。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSFXCLIENT_API FGamePlatformSFXResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX")
    EGamePlatformSFXResultCode Code = EGamePlatformSFXResultCode::InvalidRequest;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX")
    FGamePlatformSFXHandle Handle;

    bool IsAccepted() const
    {
        return Code == EGamePlatformSFXResultCode::Played ||
               Code == EGamePlatformSFXResultCode::Queued || Code == EGamePlatformSFXResultCode::AlreadyCompleted;
    }
};

/** FGamePlatformSFXDiagnostics（低频音效诊断快照）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSFXCLIENT_API FGamePlatformSFXDiagnostics
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX|Diagnostics") int32 ActiveInstances = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX|Diagnostics") int32 PendingLoads = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX|Diagnostics") int32 PeakActiveInstances = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX|Diagnostics") int64 PlayRequests = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX|Diagnostics") int64 PlayedInstances = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX|Diagnostics") int64 RejectedRequests = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX|Diagnostics") int64 StoppedInstances = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX|Diagnostics") int64 LoadFailures = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX|Diagnostics") int64 SpawnFailures = 0;
};

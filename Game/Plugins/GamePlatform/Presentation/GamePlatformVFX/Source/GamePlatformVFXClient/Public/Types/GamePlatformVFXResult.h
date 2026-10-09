// 本文件属于GamePlatform平台层 GamePlatformVFX，负责对外稳定合同/值类型；所属线程、空值、代次和所有权按相邻说明。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformVFXHandle.h"
#include "Types/GamePlatformVFXTypes.h"
#include "GamePlatformVFXResult.generated.h"

/** VFX 请求结果。Queued 表示异步加载已受理，Handle 立即可用于取消/停止。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    EGamePlatformVFXResultCode Code = EGamePlatformVFXResultCode::InvalidRequest;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    FGamePlatformVFXHandle Handle;

    bool IsAccepted() const
    {
        return Code == EGamePlatformVFXResultCode::Played || Code == EGamePlatformVFXResultCode::Queued || Code == EGamePlatformVFXResultCode::AlreadyCompleted;
    }
};

/** 客户端实例真实状态；Queued只是Loading，世界退出/内容撤销/取消/失败分别可诊断。 */
UENUM(BlueprintType)
enum class EGamePlatformVFXPlaybackState : uint8 { Invalid, Loading, Playing, Completed, Cancelled, Failed, ContentRevoked, WorldDestroyed };
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXPlaybackSnapshot
{
    FGamePlatformVFXHandle Handle;
    /** 原中立事实身份；复合子实例保留同RequestId并通过ParentHandle标识，不能误当父完成。 */
    FGuid RequestId;
    FGamePlatformVFXHandle ParentHandle;
    EGamePlatformVFXPlaybackState State = EGamePlatformVFXPlaybackState::Invalid;
    EGamePlatformVFXResultCode Code = EGamePlatformVFXResultCode::InvalidRequest;
    FName DefinitionId = NAME_None;
    /** 失败/回退诊断，不包含资源对象或租约；None/空串表示尚无实际定义或原因。 */
    FString Diagnostic;
};
DECLARE_MULTICAST_DELEGATE_OneParam(FGamePlatformVFXPlaybackCompleted, const FGamePlatformVFXPlaybackSnapshot&);

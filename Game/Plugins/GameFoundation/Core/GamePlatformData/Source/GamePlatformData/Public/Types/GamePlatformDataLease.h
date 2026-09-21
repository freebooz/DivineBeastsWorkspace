#pragma once
#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "Types/GamePlatformResult.h"
#include "GamePlatformDataLease.generated.h"

/** 资源使用期限；实例租约跨切图，世界租约绑定申请时调用者的所属世界。 */
UENUM(BlueprintType)
enum class EGamePlatformDataLifetime : uint8 { Instance, World };
/** 请求状态与资源使用期限分开；Succeeded仍持有资源，Released不可读取。 */
UENUM(BlueprintType)
enum class EGamePlatformDataRequestState : uint8 { Invalid, Loading, Succeeded, Failed, Cancelled, Released };

/** 不可伪造所有权的值句柄；所有操作核对服务内记录，副本RequestState仅为取得时快照。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMDATA_API FGamePlatformDataLease
{
    GENERATED_BODY()
    /** 游戏实例门面身份，跨实例释放被拒绝。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Data") FGuid ScopeId;
    /** 请求唯一身份，不重复利用槽位。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Data") FGuid LeaseId;
    /** 本次申请代次，异步完成必须与存活记录一致。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Data") int64 Generation = 0;
    /** 根定义主资产身份，不能作为任意文件路径加载。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Data") FPrimaryAssetId DefinitionId;
    /** 本调用者需要的去重分组，空集合仍持有根定义。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Data") TArray<FName> Bundles;
    /** 取得此值时的请求状态；实时状态使用GetLeaseState读取。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Data") EGamePlatformDataRequestState RequestState = EGamePlatformDataRequestState::Invalid;
    /** 仅检查值形状；是否仍可读由所属服务判定。 */
    bool IsValid() const { return ScopeId.IsValid() && LeaseId.IsValid() && Generation > 0 && DefinitionId.IsValid(); }
};

/** 自包含诊断，不持有定义对象或已释放指针。游戏线程取得后可复制。 */
struct GAMEPLATFORMDATA_API FGamePlatformDataDiagnostics
{
    /** 当前游戏实例作用域。 */
    FGuid ScopeId;
    /** 尚未终态的请求数。 */
    int32 PendingRequests = 0;
    /** 成功且尚未释放的租约数。 */
    int32 ActiveLeases = 0;
    /** 当前保留的终态诊断记录数；显式释放移除记录。 */
    int32 TerminalRequests = 0;
    /** 最近一次拒绝、失败或取消的值结果。 */
    FGamePlatformResult LastResult;
};

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/NoExportTypes.h"
#include "UObject/StructOpsTypeTraits.h"
#include "GamePlatformErrorCode.generated.h"

/**
 * FGamePlatformErrorCode（平台结构化错误码）。
 * 完整格式为 domain.code；Domain允许点分层级，每段及Code均为ASCII标识符。
 * 有效值比较与哈希忽略ASCII大小写并输出小写规范文本；默认空值无效，适合Fail Closed（失败关闭）。
 * 本类型只治理稳定机器错误身份，不替代领域错误枚举、用户本地化文本、HTTP状态或异常堆栈。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMCORE_API FGamePlatformErrorCode
{
    GENERATED_BODY()

    /** 错误域，可使用点分层级，例如 data.asset 或 online.auth；每段最长64字符。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Core")
    FString Domain;

    /** 域内稳定错误名称，例如 missing_definition；单段，最长64字符。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Core")
    FString Code;

    /** 校验字段和完整规范长度；不修改当前可编辑值。 */
    bool IsValid() const;

    /** 有效时返回ASCII小写 domain.code；无效时返回空字符串。 */
    FString ToString() const;

    /** 有效时返回规范FName；无效时返回NAME_None，便于渐进接入既有FGamePlatformResult.Code。 */
    FName ToName() const;

    /** 严格解析完整文本；失败时清空OutCode，拒绝空白、非ASCII、内嵌NUL和非法分段。 */
    static bool TryParse(const FString& Text, FGamePlatformErrorCode& OutCode);

    /** 从分离字段安全创建；成功输出规范小写字段，失败清空OutCode。 */
    static bool TryCreate(const FString& Domain, const FString& Code, FGamePlatformErrorCode& OutCode);

    /** 有效值按规范形式比较；无效值按原始字段精确比较。 */
    bool operator==(const FGamePlatformErrorCode& Other) const;
    bool operator!=(const FGamePlatformErrorCode& Other) const { return !(*this == Other); }
};

/** 与operator==一致的容器哈希；仅用于进程内容器，不作为协议、持久化或安全哈希。 */
GAMEPLATFORMCORE_API uint32 GetTypeHash(const FGamePlatformErrorCode& ErrorCode);

template<>
struct TStructOpsTypeTraits<FGamePlatformErrorCode> : TStructOpsTypeTraitsBase2<FGamePlatformErrorCode>
{
    enum { WithIdenticalViaEquality = true };
};

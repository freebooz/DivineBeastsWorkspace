#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/NoExportTypes.h"
#include "GamePlatformId.generated.h"

/**
 * 平台中立的逻辑身份，格式 namespace.name@version；不是资产路径、主资产ID或加载租约。
 * Namespace的每段及Name须为1..64位ASCII标识符，首位字母，其余字母、数字或下划线。
 * 完整规范字符串最多192字符；大小写按ASCII小写规范化，LogicalVersion为1..INT32_MAX。
 * 字段可编辑，因此使用前始终校验；编辑不会自动修复非法值。值拥有自身字符串，不引用UObject。
 * 只读操作可在任意线程使用独立快照；同一实例的并发读写由调用者同步，不包含网络权威。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMCORE_API FGamePlatformId
{
    GENERATED_BODY()

    /** 点分命名空间；每段非空，避免把项目资源归属硬编码进平台。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Core")
    FString Namespace;

    /** 命名空间内的单段逻辑名称，不允许点、路径分隔符或非ASCII字符。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Core")
    FString Name;

    /** 身份的逻辑代次；不同代次是不同身份，不表示插件版本或兼容承诺。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Core", meta=(ClampMin="1"))
    int32 LogicalVersion = 1;

    /** 检查全部字段及完整长度；允许ASCII大写输入，不修改本值。 */
    bool IsValid() const;

    /** 有效时返回小写规范身份；非法时返回空字符串，不拼接伪合法身份。 */
    FString ToString() const;

    /**
     * 解析完整Text，无空白裁剪、不接受正负号或十进制前导零；成功输出小写字段。
     * 失败返回false并将OutId恢复默认无效值；Text可引用OutId字段，输入读取完成后才覆盖输出。
     */
    static bool TryParse(const FString& Text, FGamePlatformId& OutId);

    /** 有效身份按ASCII小写字段和逻辑版本比较；非法值按原始字段精确比较，不合并非法身份。 */
    bool operator==(const FGamePlatformId& Other) const;

    /** 与相等比较使用同一语义，无隐式有效性或兼容性推断。 */
    bool operator!=(const FGamePlatformId& Other) const { return !(*this == Other); }
};

/** 与operator==一致的容器哈希；不作持久化、密码学或跨协议身份。放入容器后禁止原地修改键。 */
GAMEPLATFORMCORE_API uint32 GetTypeHash(const FGamePlatformId& Id);

/** 让反射容器的相等性使用规范身份比较，避免逐字段大小写比较与原生哈希不一致。 */
template<>
struct TStructOpsTypeTraits<FGamePlatformId> : TStructOpsTypeTraitsBase2<FGamePlatformId>
{
    enum { WithIdenticalViaEquality = true };
};

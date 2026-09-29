#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformResult.h"
#include "GamePlatformPCGSchema.generated.h"

/** PCG（程序化内容生成）Schema字段的逻辑值类型；只描述协议，不替代引擎Metadata真实存储类型。 */
UENUM(BlueprintType)
enum class EGamePlatformPCGAttributeValueType : uint8
{
    Name,
    Float,
    Int32,
    UInt8,
    Guid
};

/** GamePlatformPCG（游戏平台PCG）协议版本。Major变化代表不兼容Schema变更。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPCG_API FGamePlatformPCGSchemaVersion
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Schema")
    int32 Major = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Schema")
    int32 Minor = 0;

    bool IsCompatibleWith(const FGamePlatformPCGSchemaVersion& Required) const
    {
        return Major == Required.Major && Minor >= Required.Minor;
    }
};

/** 单个Pcg.*协议字段描述。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPCG_API FGamePlatformPCGAttributeDescriptor
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Schema")
    FName Name = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Schema")
    EGamePlatformPCGAttributeValueType ValueType = EGamePlatformPCGAttributeValueType::Name;

    /** 核心必需字段仅用于模板/节点合同校验；具体领域可声明更多必填字段。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Schema")
    bool bCoreRequired = false;
};

/**
 * FGamePlatformPCGAttr（PCG属性访问器）。
 * 所有平台代码统一从这里取得Pcg.*字段名，禁止业务实现散落手写属性字符串。
 */
struct GAMEPLATFORMPCG_API FGamePlatformPCGAttr
{
    static const FName BiomeId;
    static const FName BiomeWeight;
    static const FName BiomePriority;
    static const FName LayerName;
    static const FName LayerIndex;
    static const FName SpawnMeshSetId;
    static const FName SpawnClassId;
    static const FName SpawnFlags;
    static const FName RuleSlope;
    static const FName RuleHeight;
    static const FName RuleWetness;
    static const FName RuleSpanLength;
    static const FName ExcludeMask;
    static const FName ExcludeSource;
    static const FName ExecGridBand;
    static const FName ExecLodBand;
    static const FName ExecSeed;
    static const FName EnclosureKind;
    static const FName ConnectorType;
    static const FName MutableId;
};

/** Schema v1代码级稳定注册表；不使用可被内容资产任意修改的第二套Schema DataAsset。 */
class GAMEPLATFORMPCG_API FGamePlatformPCGSchema
{
public:
    static FGamePlatformPCGSchemaVersion CurrentVersion();
    static TConstArrayView<FGamePlatformPCGAttributeDescriptor> GetAttributes();
    static bool IsKnownAttribute(FName AttributeName);
    static bool IsCoreRequiredAttribute(FName AttributeName);
    static FGamePlatformResult ValidateAttributeName(FName AttributeName);
    static FGamePlatformResult ValidateRequiredAttributes(const TSet<FName>& AvailableAttributes);
};

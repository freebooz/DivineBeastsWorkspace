#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformInputProfileDefinition.h"

/**
 * FGamePlatformCompiledInputAction（编译后输入动作）。
 * Profile准备阶段一次构造，运行时只使用Slot（槽位）数组索引，避免高频GameplayTag/TMap查询。
 */
struct FGamePlatformCompiledInputAction
{
    /** 运行时紧凑槽位，等于CompiledActions数组下标。 */
    int32 Slot = INDEX_NONE;
    /** 稳定可扩展语义标识；项目层可以使用自己的GameplayTag。 */
    FGamePlatformInputSemanticId SemanticId;
    /** 单位、值类型、阻断通道和值处理策略均已在准备阶段解析。 */
    EGamePlatformInputUnit Unit = EGamePlatformInputUnit::Boolean;
    EInputActionValueType ValueType = EInputActionValueType::Boolean;
    uint8 ChannelMask = static_cast<uint8>(EGamePlatformInputChannel::Actions);
    EGamePlatformInputValuePolicy ValuePolicy = EGamePlatformInputValuePolicy::Passthrough;
    /** 兼容旧枚举调用；新项目语义可以没有Legacy语义。 */
    bool bHasLegacySemantic = false;
    EGamePlatformInputSemantic LegacySemantic = EGamePlatformInputSemantic::Move;
    /** Data Input Bundle已经加载的原生动作；这里只保留弱引用，不额外拥有资产。 */
    TWeakObjectPtr<UInputAction> Action;
};

/**
 * 将已加载Profile动作声明一次性编译成紧凑运行时表。
 * OutSlotByTag和OutLegacySlot仅用于低频准备/Touch入口查找，高频Enhanced Input回调直接捕获Slot。
 */
FGamePlatformResult CompileGamePlatformInputProfile(
    const UGamePlatformInputProfileDefinition& Profile,
    TArray<FGamePlatformCompiledInputAction>& OutCompiledActions,
    TMap<FGameplayTag, int32>& OutSlotByTag,
    TMap<uint8, int32>& OutLegacySlot);

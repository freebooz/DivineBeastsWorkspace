// 本文件属于GamePlatform平台层 GamePlatformVFX，负责对外稳定合同/值类型；所属线程、空值、代次和所有权按相邻说明。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformVFXTypes.h"
#include "GamePlatformVFXCatalogTypes.generated.h"

class UGamePlatformVFXDefinition;

/** 客户端旧工具目录的稳定映射合同；仅保存资格和逻辑身份，不拥有加载租约或玩法权威。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXCatalogEntry
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FGameplayTag SemanticTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FName ContextId = NAME_None;

    /** 英雄、技能、皮肤、世界的等值资格；None表示未约束，每个非空且匹配项计具体度一项。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX") FName HeroDefinitionId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX") FName AbilityId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX") FName SkinId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX") FName WorldId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FGameplayTagContainer RequiredContextTags;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FGameplayTagContainer BlockedContextTags;

    /** 上游Presentation或专门工具可直接使用的逻辑Definition ID。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FName DefinitionId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    TSoftObjectPtr<UGamePlatformVFXDefinition> Definition;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    int32 Priority = 0;

    /** 保留旧序列化字段；手填权重不再参与P13具体度，迁移为上述等值资格。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX", meta=(DeprecatedProperty, DeprecationMessage="使用类型化等值约束；手填Specificity不参与排序"))
    int32 Specificity = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    EGamePlatformVFXCatalogScope Scope = EGamePlatformVFXCatalogScope::Platform;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FName PlatformId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    bool bAnyQuality = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX", meta=(EditCondition="!bAnyQuality"))
    EGamePlatformVFXQualityTier QualityTier = EGamePlatformVFXQualityTier::High;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    bool bFallback = false;
};

// 本文件属于MobaCommon可选MOBA层 MobaPresentation，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// MOBA共享语义转平台中立请求；保持事实源身份，既不读取项目资产也不执行客户端播放。
#include "MobaPresentationRequestBuilder.h"

FGamePlatformPresentationRequest FMobaPresentationRequestBuilder::Build(
    const FMobaPresentationAdaptedFact& Fact,
    int32 RequestGeneration)
{
    FGamePlatformPresentationRequest Request;
    Request.RequestId = Fact.Identity.FactId.IsValid()
        ? Fact.Identity.FactId
        : FGuid::NewGuid();
    Request.SemanticTag = Fact.Semantic;
    Request.ContextId = !Fact.Context.ArenaModeId.IsNone()
        ? Fact.Context.ArenaModeId
        : Fact.Context.TeamId;
    Request.SourceId = Fact.Context.SourceEntityId.IsEmpty()
        ? NAME_None
        : FName(*Fact.Context.SourceEntityId);
    Request.TargetId = Fact.Context.TargetEntityId.IsEmpty()
        ? NAME_None
        : FName(*Fact.Context.TargetEntityId);
    Request.SourceLocation = Fact.Context.SourceLocation;
    Request.TargetLocation = Fact.Context.TargetLocation;
    Request.ImpactLocation = Fact.Context.ImpactLocation;
    Request.ImpactNormal = Fact.Context.ImpactNormal;
    Request.Magnitude = Fact.Context.Magnitude;
    Request.ContextTags = Fact.Context.AdditionalTags;
    Request.Priority = Fact.Priority;
    Request.Lifetime = Fact.Lifetime;
    Request.PredictionState = Fact.Identity.bPredicted && !Fact.Identity.bConfirmed
        ? EGamePlatformPresentationPredictionState::Predicted
        : EGamePlatformPresentationPredictionState::Confirmed;
    Request.WorldGeneration = Fact.Context.WorldGeneration;
    Request.RequestGeneration = RequestGeneration;
    Request.Context.HeroDefinitionId = Fact.Context.HeroDefinitionId.IsEmpty() ? NAME_None : FName(*Fact.Context.HeroDefinitionId);
    Request.Context.AbilityId = Fact.Context.AbilityId.IsEmpty() ? NAME_None : FName(*Fact.Context.AbilityId);
    Request.Context.ArenaModeId = Fact.Context.ArenaModeId;
    Request.Context.AvatarGeneration = Fact.Context.AvatarGeneration;
    Request.Context.WorldGeneration = Fact.Context.WorldGeneration;
    return Request;
}

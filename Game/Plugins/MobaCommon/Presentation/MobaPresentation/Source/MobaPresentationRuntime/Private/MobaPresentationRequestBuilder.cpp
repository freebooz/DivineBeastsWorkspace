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
    return Request;
}

#include "Scenarios/DivineBeastsDemoPresentationScenarios.h"
#include "Types/GamePlatformSFXTypes.h"
namespace DivineBeasts::Demo
{
struct FPresentationParameterSignature {FName Feature;TArray<FName> Names;TArray<EDivineBeastsDemoParameterType> Types;};
#include "Scenarios/DivineBeastsDemoPresentationSignatures.inl"
bool GetPresentationManualParameterSignature(FName Feature,TArray<FName>& Names,TArray<EDivineBeastsDemoParameterType>& Types)
{for(const auto& S:PresentationParameterSignatures())if(S.Feature==Feature){Names=S.Names;Types=S.Types;return true;}Names.Reset();Types.Reset();return false;}
TArray<FName> PresentationManualFeatures()
{
    return {TEXT("GamePlatformPresentation.catalog"),TEXT("GamePlatformPresentation.routing"),TEXT("GamePlatformPresentation.preview"),
        TEXT("GamePlatformVFX.ten_behaviors"),TEXT("GamePlatformVFX.preload"),TEXT("GamePlatformVFX.quality_prediction"),TEXT("GamePlatformVFX.fauna"),TEXT("GamePlatformVFX.dissolve"),
        TEXT("GamePlatformSFX.spaces"),TEXT("GamePlatformSFX.parameters"),TEXT("GamePlatformSFX.lifecycle_prediction"),TEXT("GamePlatformSFX.user_volume"),
        TEXT("GamePlatformCamera.cinematic"),TEXT("GamePlatformCamera.hit_shake"),TEXT("GamePlatformUI.screens"),TEXT("GamePlatformUI.theme"),
        TEXT("GamePlatformUI.adaptive"),TEXT("GamePlatformUI.notification_feedback"),TEXT("GamePlatformUI.world_projection"),TEXT("GamePlatformSurface.environment"),TEXT("GamePlatformSurface.snow_contact"),TEXT("GamePlatformSurface.marks"),TEXT("GamePlatformSurface.editor_wind")};
}
TArray<FName> PresentationManualOperations(FName F)
{
    if(F==TEXT("GamePlatformPresentation.catalog"))return {TEXT("Register"),TEXT("Resolve"),TEXT("Conflict"),TEXT("Withdraw"),TEXT("Stop")};
    if(F==TEXT("GamePlatformPresentation.routing"))return {TEXT("Submit"),TEXT("Confirm"),TEXT("Cancel"),TEXT("Stale"),TEXT("Stop")};
    if(F==TEXT("GamePlatformPresentation.preview"))return {TEXT("LoadPreview"),TEXT("Rotate"),TEXT("Distance"),TEXT("Reset"),TEXT("Stop")};
    if(F==TEXT("GamePlatformVFX.preload"))return {TEXT("Preload"),TEXT("InspectPreload"),TEXT("CancelPreload"),TEXT("Play"),TEXT("Reset"),TEXT("Stop")};
    if(F==TEXT("GamePlatformVFX.fauna"))return {TEXT("Disturb"),TEXT("Snapshot"),TEXT("Stop")};
    if(F==TEXT("GamePlatformVFX.dissolve"))return {TEXT("LoadPreview"),TEXT("Dissolve"),TEXT("Duplicate"),TEXT("RefreshMeshes"),TEXT("Reset"),TEXT("Stop")};
    if(F==TEXT("GamePlatformVFX.ten_behaviors")||F==TEXT("GamePlatformVFX.quality_prediction"))return {TEXT("Play"),TEXT("Predict"),TEXT("Confirm"),TEXT("Correct"),TEXT("Cancel"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
    if(F==TEXT("GamePlatformSFX.user_volume"))return {TEXT("SetVolume"),TEXT("Commit"),TEXT("Reset"),TEXT("Stop")};
    if(F.ToString().StartsWith(TEXT("GamePlatformSFX.")))return {TEXT("Play"),TEXT("Predict"),TEXT("Confirm"),TEXT("Correct"),TEXT("Cancel"),TEXT("SetParameter"),TEXT("SetGain"),TEXT("Snapshot"),TEXT("Reset"),TEXT("Stop")};
    if(F==TEXT("GamePlatformCamera.cinematic"))return {TEXT("BeginView"),TEXT("EndView"),TEXT("InvalidTarget"),TEXT("Stop")};
    if(F==TEXT("GamePlatformCamera.hit_shake"))return {TEXT("Shake"),TEXT("Duplicate"),TEXT("Comfort"),TEXT("Reset"),TEXT("Stop")};
    if(F==TEXT("GamePlatformUI.screens"))return {TEXT("Open"),TEXT("CancelOpen"),TEXT("Close"),TEXT("Reset"),TEXT("Stop")};
    if(F==TEXT("GamePlatformUI.theme"))return {TEXT("InspectTheme"),TEXT("SwitchTheme"),TEXT("CancelTheme"),TEXT("RejectInvalidTheme"),TEXT("Stop")};
    if(F==TEXT("GamePlatformUI.adaptive"))return {TEXT("RefreshAdaptive"),TEXT("Stop")};
    if(F==TEXT("GamePlatformUI.notification_feedback"))return {TEXT("Notify"),TEXT("Feedback"),TEXT("Dismiss"),TEXT("Stop")};
    if(F==TEXT("GamePlatformUI.world_projection"))return {TEXT("RegisterWorldUI"),TEXT("MoveWorldUI"),TEXT("HideWorldUI"),TEXT("RemoveWorldUI"),TEXT("Stop")};
    if(F==TEXT("GamePlatformSurface.environment"))return {TEXT("ApplyState"),TEXT("RepeatState"),TEXT("RejectNonFinite"),TEXT("RefreshBinding"),TEXT("Reset"),TEXT("Stop")};
    if(F==TEXT("GamePlatformSurface.snow_contact"))return {TEXT("AttachFeet"),TEXT("DetachFeet"),TEXT("Stop")};
    if(F==TEXT("GamePlatformSurface.marks"))return {TEXT("SubmitMark"),TEXT("DuplicateMark"),TEXT("Counts"),TEXT("Stop")};
    if(F==TEXT("GamePlatformSurface.editor_wind"))return {TEXT("CreateEditorWorld"),TEXT("ReadWind"),TEXT("ApplyWind"),TEXT("Reset"),TEXT("Stop")};
    return {};
}
bool ValidatePresentationManualContract(const FDivineBeastsDemoScenarioDefinition& D)
{
    if(!PresentationManualFeatures().Contains(D.FeatureId)||D.AdapterId!=FName(*(TEXT("PlatformPresentation.")+D.FeatureId.ToString()))||
        D.ScenarioId!=FName(*(D.FeatureId.ToString()+TEXT(".Manual"))))return false;
    const auto Allowed=PresentationManualOperations(D.FeatureId);
    TArray<FName> Names;TArray<EDivineBeastsDemoParameterType> Types;
    if(!GetPresentationManualParameterSignature(D.FeatureId,Names,Types)||D.Parameters.Num()!=Names.Num())return false;
    for(int32 I=0;I<Names.Num();++I)if(D.Parameters[I].ParameterId!=Names[I]||D.Parameters[I].DefaultValue.Type!=Types[I])return false;
    if(D.Operations.Num()!=Allowed.Num())return false;
    TSet<FName> Seen;
    for(FName Op:D.Operations)if(!Allowed.Contains(Op)||Seen.Contains(Op))return false;else Seen.Add(Op);
    return true;
}
bool PresentationManualSameSFXHandle(const FGamePlatformSFXHandle& A,const FGamePlatformSFXHandle& B)
{return A.Id==B.Id && A.Generation==B.Generation && A.World==B.World;}
bool CanBeginPresentationThemeFixture(const FGamePlatformUIThemeSnapshot& S)
{return S.ScopeId.IsValid()&&S.ThemeId.IsValid()&&S.Context.IsValid()&&!S.bRequestPending;}
FGamePlatformResult ValidatePresentationManualValues(const FDivineBeastsDemoScenarioDefinition& D,const TArray<FDivineBeastsDemoParameterValue>& V)
{
    if(V.Num()!=D.Parameters.Num())return FGamePlatformResult::Failure(TEXT("Demo.Presentation.ParameterCount"),TEXT("参数数量与冻结合同不匹配。"));
    for(int32 I=0;I<V.Num();++I)
    {
        const auto& P=D.Parameters[I];
        if(V[I].Type!=P.DefaultValue.Type||
            (V[I].Type==EDivineBeastsDemoParameterType::Number&&(!FMath::IsFinite(V[I].NumberValue)||V[I].NumberValue<P.Minimum||V[I].NumberValue>P.Maximum))||
            (V[I].Type==EDivineBeastsDemoParameterType::Integer&&(V[I].IntegerValue<P.Minimum||V[I].IntegerValue>P.Maximum)))
            return FGamePlatformResult::Failure(TEXT("Demo.Presentation.Parameter"),TEXT("参数类型、有限值或数值范围不匹配。"));
    }
    return FGamePlatformResult::Success();
}
}

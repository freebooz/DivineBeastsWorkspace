#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "Scenarios/DivineBeastsDemoPresentationManualState.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
namespace DivineBeasts::Demo::Tests
{
inline bool FindStandalone(FAutomationTestBase& Test,UWorld*& World,ULocalPlayer*& Player)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("PresentationManualNative")))
    {Test.AddError(TEXT("必须由PresentationLive gate启动独占Standalone ClientContext夹具。"));return false;}
    World=nullptr;Player=nullptr;
    if(GEngine)for(const auto& C:GEngine->GetWorldContexts())if(auto* W=C.World();W&&W->WorldType==EWorldType::Game&&W->GetNetMode()==NM_Standalone&&!W->bIsTearingDown&&W->GetGameInstance())
    {if(auto* P=W->GetGameInstance()->GetFirstGamePlayer()){World=W;Player=P;break;}}
    if(!World||!Player){Test.AddError(TEXT("真实Game/Standalone LP不存在；不跳过，不初始化替身GI。"));return false;}return true;
}
inline TSharedPtr<FPresentationManualState> MakeState(FAutomationTestBase& Test,FName Feature)
{
    UWorld* W=nullptr;ULocalPlayer* P=nullptr;if(!FindStandalone(Test,W,P))return {};
    auto S=MakeShared<FPresentationManualState>();S->World=W;S->Player=P;S->Feature=Feature;
    S->Context.Token.RunId=FGuid::NewGuid();S->Context.Token.WorldId=FGuid::NewGuid();S->Context.Token.Generation=1;S->Context.World=W;
    S->Host=W->GetGameInstance()->GetSubsystem<UDivineBeastsDemoManualHostSubsystem>();
    if(!S->Host.IsValid()||!S->Host->Acquire(S->Context.Token,*W).IsSuccess()){Test.AddError(TEXT("真实排他ManualHost不可用。"));return {};}
    S->bHost=true;S->bBound=true;S->bActive=true;
    if(auto* UI=S->Local<UGamePlatformUIManagerSubsystem>()){S->ThemeSource=UI;S->OriginalTheme=UI->GetThemeSnapshot();S->ThemeExpectedRevision=S->OriginalTheme.Revision;}
    S->BindEvents();return S;
}
inline TArray<FDivineBeastsDemoParameterValue> ThemeValues(FPresentationManualState& S)
{
    S.Context.Scenario.Parameters.Reset();TArray<FDivineBeastsDemoParameterValue> V;
    auto Add=[&](FName Name,FDivineBeastsDemoParameterValue Value){FDivineBeastsDemoParameterDefinition P;P.ParameterId=Name;P.DefaultValue.Type=Value.Type;S.Context.Scenario.Parameters.Add(P);V.Add(Value);};
    const FString Canonical=S.OriginalTheme.ThemeId.PrimaryAssetName.ToString();FString Base,Version;Canonical.Split(TEXT("@"),&Base,&Version,ESearchCase::CaseSensitive,ESearchDir::FromEnd);
    FDivineBeastsDemoParameterValue X;X.Type=EDivineBeastsDemoParameterType::Identifier;X.IdentifierValue=FName(*Base);Add(TEXT("ThemeId"),X);
    X={};X.Type=EDivineBeastsDemoParameterType::Integer;X.IntegerValue=FCString::Atoi(*Version);Add(TEXT("ThemeVersion"),X);
    X={};X.Type=EDivineBeastsDemoParameterType::Identifier;X.IdentifierValue=S.OriginalTheme.Context.PlatformId;Add(TEXT("Platform"),X);
    X.IdentifierValue=S.OriginalTheme.Context.SkinId.IsNone()?FName(TEXT("Default")):S.OriginalTheme.Context.SkinId;Add(TEXT("Skin"),X);
    X={};X.Type=EDivineBeastsDemoParameterType::Integer;X.IntegerValue=S.OriginalTheme.Context.QualityTier;Add(TEXT("ThemeQuality"),X);return V;
}
}
#endif

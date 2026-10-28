// 私有运行状态：只拥有本次观察者、平台句柄、中央Data租约和临时场景对象。
#pragma once
#include "Scenarios/DivineBeastsDemoPresentationScenarios.h"
#include "Scenarios/DivineBeastsDemoPresentationObserver.h"
#include "Scenarios/DivineBeastsDemoManualHostSubsystem.h"
#include "Types/GamePlatformDataLease.h"
#include "Types/GamePlatformVFXRequest.h"
#include "Types/GamePlatformVFXResult.h"
#include "Types/GamePlatformVFXPreloadHandle.h"
#include "Types/GamePlatformSFXUserSettingsTypes.h"
#include "Types/GamePlatformSurfaceTypes.h"
#include "Marks/GamePlatformSurfaceMarkTypes.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Requests/GamePlatformWorldUIRequest.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
class UGamePlatformCinematicCameraSubsystem;
class UGamePlatformCameraHitFeedbackSubsystem;
class UGamePlatformUIManagerSubsystem;
class UGamePlatformUIAdaptiveSubsystem;
class UGamePlatformMeshDissolveComponent;
class UGamePlatformSnowFootContactComponent;
class AGamePlatformCharacterPreviewStage;
class ULocalPlayer;
class IGamePlatformDataService;
class ADivineBeastsWorldPlayerController;
class UCommonActivatableWidgetStack;
class UGamePlatformUIScreenDefinition;
namespace DivineBeasts::Demo
{
inline FGamePlatformResult PresentationError(const TCHAR* Code,const TCHAR* Text)
{return FGamePlatformResult::Failure(FName(Code),Text);}
/** 参数按冻结定义解码；构造前必须校验完整数量、类型和数值范围。 */
struct FPresentationArgs
{
    const FDivineBeastsDemoScenarioContext& C;const TArray<FDivineBeastsDemoParameterValue>& V;
    const FDivineBeastsDemoParameterValue* Find(FName Id)const
    {for(int32 I=0;I<C.Scenario.Parameters.Num()&&I<V.Num();++I)if(C.Scenario.Parameters[I].ParameterId==Id)return &V[I];return nullptr;}
    FName Id(FName Key)const{auto* X=Find(Key);return X?X->IdentifierValue:NAME_None;}
    double Number(FName Key,double Default=0)const{auto* X=Find(Key);return X?X->NumberValue:Default;}
    int32 Int(FName Key,int32 Default=0)const{auto* X=Find(Key);return X?static_cast<int32>(X->IntegerValue):Default;}
    FSoftObjectPath Asset(FName Key)const{auto* X=Find(Key);return X?X->AssetPathValue:FSoftObjectPath();}
    FName Definition()const{return FName(*(Id(TEXT("DefinitionId")).ToString()+TEXT("@")+FString::FromInt(Int(TEXT("DefinitionVersion"),1))));}
    FVector Location()const{return FVector(Number(TEXT("X")),Number(TEXT("Y")),Number(TEXT("Z"),100));}
};
struct FPresentationManualState : TSharedFromThis<FPresentationManualState>
{
    FDivineBeastsDemoScenarioContext Context;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ULocalPlayer> Player;
    TWeakObjectPtr<ADivineBeastsWorldPlayerController> CameraController;
    TWeakObjectPtr<UDivineBeastsDemoManualHostSubsystem> Host;
    TStrongObjectPtr<UDivineBeastsDemoPresentationObserver> Observer;
    FName Feature;
    bool bBound=false,bActive=false,bCleaning=false,bHost=false;
    uint64 Epoch=1;
    FDelegateHandle WorldCleanup,VFXCompleted,SFXCompleted,VolumesChanged,SurfaceChanged,RequestObserved,MarkSpawned,FinalCameraChanged;
    FGamePlatformDataLease AssetLease;
    FDivineBeastsDemoOperationCompletion LoadingCompletion;
    TArray<FGamePlatformVFXHandle> VFXHandles;
    TArray<FGamePlatformSFXHandle> SFXHandles;
    TArray<FGuid> VFXRequests,SFXRequests;
    FGamePlatformVFXPreloadHandle Preload;
    FGamePlatformVFXRequest LastVFX;
    FGamePlatformSFXRequest LastSFX;
    FGamePlatformPresentationRequest LastPresentation;
    TArray<FGamePlatformPresentationRequest> PresentationRequests;
    TArray<FGuid> MarkRequests;
    FGamePlatformSurfaceMarkRequest LastMark;
    FGamePlatformPresentationRegistrationHandle Catalog,Contributor;
    TWeakObjectPtr<AGamePlatformCharacterPreviewStage> Stage;
    TWeakObjectPtr<UGamePlatformMeshDissolveComponent> Dissolve;
    TWeakObjectPtr<UGamePlatformSnowFootContactComponent> Feet;
    int32 DissolveGeneration=0;
    float OriginalShakeScale=1;
    bool bShakeScaleCaptured=false,bVolumesCaptured=false,bSurfaceCaptured=false;
    FGamePlatformSFXUserVolumes OriginalVolumes;
    FGamePlatformSurfaceEnvironmentState OriginalSurface,LastSurface;
    TWeakObjectPtr<UGamePlatformUIScreen> Screen;
    TWeakObjectPtr<UCommonActivatableWidgetStack> ScreenStack;
    TWeakObjectPtr<UGamePlatformUIScreenDefinition> RegisteredDefinition;
    struct FScreenTerminal {FGuid Request;FName ScreenId;bool bSuccess=false;TWeakObjectPtr<UGamePlatformUIScreen> Instance;FText Reason;};
    bool bSubmittingOpen=false,bOpenBufferOverflow=false;
    TArray<FScreenTerminal> SynchronousOpenTerminals;
    FName RegisteredScreen;
    FGuid Opening,ThemeRequest,Notification,Feedback,WorldUI,ShakeEvent;
    FGamePlatformUIThemeSnapshot OriginalTheme;
    FPrimaryAssetId RequestedTheme;
    FGamePlatformUIThemeContext RequestedThemeContext;
    TWeakObjectPtr<UGamePlatformUIManagerSubsystem> ThemeSource;
    FGuid ThemeRestore;
    int64 ThemeExpectedRevision=0;
    bool bThemeChanged=false;
    FGamePlatformWorldUIRequest WorldUIRequest;
    bool bOwnView=false;
#if WITH_EDITOR
    TWeakObjectPtr<UWorld> EditorWindWorld;
    FGamePlatformSurfaceWindTransition ExpectedEditorWind;
    FGamePlatformSurfaceWindTransition OriginalEditorWind;
    bool bEditorWindCaptured=false;
    bool bEditorWindOperation=false;
#endif
    bool Alive()const;
    bool Current(uint64 E)const{return Alive()&&Epoch==E;}
    UGameInstance* Instance()const;
    IGamePlatformDataService* Data()const;
    template<class T>T* Local()const {auto* P=Player.Get();return P?P->GetSubsystem<T>():nullptr;}
    void Observe(FString Text)const;
    FGamePlatformResult Prepare();
    void Execute(FName Op,const FPresentationArgs& A,FDivineBeastsDemoOperationCompletion Done);
    FGamePlatformResult Cleanup();
    void BindEvents();
    void UnbindEvents();
    void Load(const FSoftObjectPath& Path,TFunction<FGamePlatformResult(UObject*)> Apply,FDivineBeastsDemoOperationCompletion Done);
    FGamePlatformResult ReleaseAsset();
    FGamePlatformResult ResetPlayback();
    FGamePlatformResult ResetStage();
    FGamePlatformResult OpenOwnedScreen(UGamePlatformUIScreenDefinition& Definition);
    FGamePlatformResult CloseOwnedScreen();
    void OnScreenTerminal(const FScreenTerminal& Terminal);
    void ReconcileOwnedScreen();
    void OnOwnedThemeTerminal(FGuid Request,bool bSuccess);
    void ExecuteVFX(FName Op,const FPresentationArgs& A,FDivineBeastsDemoOperationCompletion Done);
    void ExecuteSFX(FName Op,const FPresentationArgs& A,FDivineBeastsDemoOperationCompletion Done);
    void ExecutePresentation(FName Op,const FPresentationArgs& A,FDivineBeastsDemoOperationCompletion Done);
    void ExecuteCamera(FName Op,const FPresentationArgs& A,FDivineBeastsDemoOperationCompletion Done);
    void ExecuteUI(FName Op,const FPresentationArgs& A,FDivineBeastsDemoOperationCompletion Done);
    void ExecuteSurface(FName Op,const FPresentationArgs& A,FDivineBeastsDemoOperationCompletion Done);
    FGamePlatformResult ExecuteEditorWind(FName Op,const FPresentationArgs& A);
    FGamePlatformResult CloseEditorWind();
};
}

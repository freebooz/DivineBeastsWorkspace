// 只承接动态委托到本次适配弱状态；无Tick、计时器或业务状态。
#pragma once
#include "CoreMinimal.h"
#include "GamePlatformUITypes.h"
#include "UObject/Object.h"
#include "DivineBeastsDemoPresentationObserver.generated.h"
class UGamePlatformUIScreen;
UCLASS(Transient)
class UDivineBeastsDemoPresentationObserver final : public UObject
{
    GENERATED_BODY()
public:
    TFunction<void(FGuid,FName,UGamePlatformUIScreen*)> Opened;
    TFunction<void(FGuid,FName,FText)> OpenFailed;
    TFunction<void(FName)> Closed;
    TFunction<void(FGuid,bool,FText)> ThemeFinished;
    TFunction<void(FGamePlatformUIAdaptiveContext)> Adaptive;
    UFUNCTION() void OnOpened(FGuid Request,FName Id,UGamePlatformUIScreen* Screen) {if(Opened)Opened(Request,Id,Screen);}
    UFUNCTION() void OnOpenFailed(FGuid Request,FName Id,FText Reason) {if(OpenFailed)OpenFailed(Request,Id,Reason);}
    UFUNCTION() void OnClosed(FName Id) {if(Closed)Closed(Id);}
    UFUNCTION() void OnThemeFinished(FGuid Request,bool Success,FText Reason) {if(ThemeFinished)ThemeFinished(Request,Success,Reason);}
    UFUNCTION() void OnAdaptive(FGamePlatformUIAdaptiveContext Value) {if(Adaptive)Adaptive(Value);}
    void Clear() {Opened={};OpenFailed={};Closed={};ThemeFinished={};Adaptive={};}
};

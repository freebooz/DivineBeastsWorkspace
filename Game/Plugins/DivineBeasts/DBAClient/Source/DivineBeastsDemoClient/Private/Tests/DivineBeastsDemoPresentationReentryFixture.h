// 原生生命周期夹具：真实CreateWidget/Manager/Data执行，不生成可交付视觉布局。
#pragma once
#include "Screens/GamePlatformUIScreen.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "DivineBeastsDemoPresentationReentryFixture.generated.h"
UCLASS(Transient,NotBlueprintable)
class UDivineBeastsDemoPresentationTestScreen final:public UGamePlatformUIScreen
{
    GENERATED_BODY()
public:bool CloseByNativeBack(){return NativeOnHandleBackAction();}
};
UCLASS(Transient,NotBlueprintable)
class UDivineBeastsDemoPresentationEmptyRoot final:public UGamePlatformUILayerStack
{GENERATED_BODY()};
UCLASS(Transient,NotBlueprintable)
class UDivineBeastsDemoPresentationTestObserver final:public UObject
{
    GENERATED_BODY()
public:
    TFunction<void(int64)> Changed;
    TFunction<void(FGuid,bool)> Finished;
    TFunction<void(FGuid,FName,UGamePlatformUIScreen*)> Opened;
    UFUNCTION()void OnChanged(int64 Revision){if(Changed)Changed(Revision);}
    UFUNCTION()void OnFinished(FGuid Id,bool Success,FText Reason){if(Finished)Finished(Id,Success);}
    UFUNCTION()void OnOpened(FGuid Id,FName Name,UGamePlatformUIScreen* Screen){if(Opened)Opened(Id,Name,Screen);}
    void Clear(){Changed={};Finished={};Opened={};}
};

// 仅将真实组件事件投影给本Run；不拥有业务权威或定时采样。
#pragma once
#include "CoreMinimal.h"
#include "Types/GamePlatformInteractionFocusSnapshot.h"
#include "Types/GamePlatformInteractionSession.h"
#include "Types/GamePlatformInteractionResult.h"
#include "Types/GamePlatformInteractionEvent.h"
#include "Components/GamePlatformWaterInteractorComponent.h"
#include "DivineBeastsDemoWorldObserver.generated.h"
UCLASS(Transient)
class UDivineBeastsDemoWorldObserver final : public UObject
{
 GENERATED_BODY()
public:
 TFunction<void(FName)> Changed;
 void Notify(FName Event){auto Copy=Changed;if(Copy)Copy(Event);}
 UFUNCTION() void Focus(const FGamePlatformInteractionFocusSnapshot& Value){Notify(TEXT("FocusChanged"));}
 UFUNCTION() void Session(const FGamePlatformInteractionSession& Value){Notify(TEXT("SessionChanged"));}
 UFUNCTION() void Result(const FGamePlatformInteractionResult& Value){Notify(TEXT("ResultChanged"));}
 UFUNCTION() void Interaction(const FGamePlatformInteractionEvent& Value){Notify(TEXT("InteractionEvent"));}
 UFUNCTION() void Target(){Notify(TEXT("TargetStateChanged"));}
 UFUNCTION() void Water(const FGamePlatformWaterPresence& Value){Notify(TEXT("WaterPresenceChanged"));}
};

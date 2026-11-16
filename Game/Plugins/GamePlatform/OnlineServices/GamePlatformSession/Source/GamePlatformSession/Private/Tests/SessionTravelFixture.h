#pragma once
#include "GameFramework/PlayerController.h"
#include "SessionTravelFixture.generated.h"

/** 原生测试的网络旅行边界替身；不建立连接，不发送真实票据，不进入发布内容。 */
UCLASS(Transient, NotBlueprintable)
class ASessionTravelFixture final : public APlayerController
{
    GENERATED_BODY()
public:
    int32 TravelCalls = 0;
    virtual void ClientTravelInternal_Implementation(const FString& URL, ETravelType TravelType,
        bool bSeamless, FGuid MapPackageGuid) override;
};

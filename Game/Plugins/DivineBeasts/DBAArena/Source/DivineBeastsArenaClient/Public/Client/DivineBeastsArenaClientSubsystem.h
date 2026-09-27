#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Client/GamePlatformArenaClientTypes.h"
#include "DivineBeastsArenaClientSubsystem.generated.h"

class IDivineBeastsApplicationFlowExtension;
class UDivineBeastsApplicationFlowSubsystem;

/**
 * UDivineBeastsArenaClientSubsystem（神兽联盟竞技客户端适配子系统）。
 * 只做项目模式元数据、ApplicationFlow扩展装配和PostMatch返回；不实现UI/匹配算法/服务器规则。
 */
UCLASS()
class DIVINEBEASTSARENACLIENT_API UDivineBeastsArenaClientSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure, Category="DivineBeasts|Arena")
    TArray<FName> GetProjectArenaModeIds() const;

    bool BuildMatchmakingRequest(
        FName ArenaModeId,
        const FString& PartyId,
        const FString& PreferredRegion,
        const FString& ClientRequestId,
        FGamePlatformArenaMatchmakingRequest& OutRequest,
        FString& OutError) const;

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Arena")
    bool RequestPostMatchReturnToWorld();

    bool IsApplicationFlowInWorld() const { return bApplicationFlowInWorld; }

    void SetApplicationFlowInWorld(bool bInWorld)
    {
        bApplicationFlowInWorld = bInWorld;
    }

private:
    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsApplicationFlowSubsystem> ApplicationFlow = nullptr;

    TSharedPtr<IDivineBeastsApplicationFlowExtension> FlowExtension;
    bool bApplicationFlowInWorld = false;
};

#pragma once
#include "GameFramework/Actor.h"
#include "Interfaces/GamePlatformInteractable.h"
#include "DivineBeastsArenaEntrance.generated.h"

class UDivineBeastsArenaEntranceDefinition;
class UGamePlatformInteractableComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UBoxComponent;

/** 新手村共享活动入口。服务器只确认交互事实，客户端可选竞技适配消费；不直接旅行或创建比赛。 */
UCLASS()
class DBAWORLDSRUNTIME_API ADivineBeastsArenaEntrance final : public AActor, public IGamePlatformInteractable
{
    GENERATED_BODY()
public:
    ADivineBeastsArenaEntrance();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    /** 放置数据及其复制身份均由服务器/地图拥有；客户端不能通过按钮改写入口模式。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Definition, Category="DivineBeasts|Activity")
    TObjectPtr<UDivineBeastsArenaEntranceDefinition> EntranceDefinition;
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Activity")
    FName GetArenaModeId() const;
    virtual bool CanBeginInteraction(const FGamePlatformInteractionSession& Session,const FGamePlatformInteractionOption& Option) const override;
    virtual bool CommitInteraction(const FGamePlatformInteractionSession& Session,const FGamePlatformInteractionOption& Option) override;
    /** 纯显示组件，网格材质由Village内容包配置；门洞碰撞由独立立柱提供，不能用包围盒堵住门洞。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="DivineBeasts|Activity") TObjectPtr<UStaticMeshComponent> Frame;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="DivineBeasts|Activity") TObjectPtr<UStaticMeshComponent> Surface;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="DivineBeasts|Activity") TObjectPtr<UTextRenderComponent> Title;
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UGamePlatformInteractableComponent> Interactable;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> ClickSurface;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> LeftPillar;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> RightPillar;
    UFUNCTION() void OnRep_Definition();
    void RefreshDefinition();
};

// 项目私有开发装配。唯一World服务拥有初始化/定义租约；本宿主仅拥有Provider和通知登记。
#pragma once
#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformRegion.h"
#include "Contracts/DivineBeastsDemoRunTypes.h"
#include "DivineBeastsDemoManualWorldHostSubsystem.generated.h"
class UDivineBeastsDemoWorldSubsystem;
UCLASS(Transient)
class UDivineBeastsDemoManualWorldHostSubsystem final:public UWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
 virtual bool DoesSupportWorldType(EWorldType::Type Type) const override { return Type==EWorldType::Game; }
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void OnWorldBeginPlay(UWorld& World) override;
 virtual void OnWorldEndPlay(UWorld& World) override;
 virtual void Deinitialize() override;
 /** 只读诊断；失败不会伪造Session或接管外部Provider。 */
 FGamePlatformResult GetLastResult() const {return LastResult;}
 /** 准备前同步收敛交接；仅同World真实Run，可在排他ManualHost中校验。 */
 FGamePlatformResult PrepareRun(const FDivineBeastsDemoRunToken& Token,FName Feature);
private:
 void RunChanged(const FDivineBeastsDemoRunSnapshot& Value);
 void QueueReconcile();
 void Reconcile(bool bAllowDeferred=false);
 void Close();
 TWeakObjectPtr<UDivineBeastsDemoWorldSubsystem> Demo;
 FDelegateHandle RunSubscription;
 FGamePlatformWorldRegistration StateSubscription,Provider;
 FGamePlatformResult LastResult;
 FGuid WorldGeneration;
 uint64 Epoch=1;
 bool bClosing=false,bInitialized=false,bAttempted=false,bRegionPage=false,bQueued=false,bReconciling=false;
};

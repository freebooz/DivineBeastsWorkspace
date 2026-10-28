// SSLM项目Editor原生回归：真实中央runner创建、供应商设置CAS冲突、真实EndPIE和Editor保留清理。
// 测试不持Adapter/GI/Runner强引用，不伪造World；保留外部值后再精确恢复本测试自有修改。
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
#include "Manual/GamePlatformEditorResources.h"
#include "BlueprintAutoLayoutManual.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
namespace {
class FEditorCentralRetainedCleanup final:public IAutomationLatentCommand {
 FAutomationTestBase* T;double Deadline=FPlatformTime::Seconds()+60;int32 Stage=0;
 TWeakObjectPtr<UDivineBeastsDemoWorldSubsystem> Runner;FDivineBeastsDemoScenarioDefinition Scenario;
 TArray<FDivineBeastsDemoParameterValue> Values;FGamePlatformEditorResourceLease Lease;
 BlueprintAutoLayoutManual::FSettingsSnapshot Original,Written,External;
public:explicit FEditorCentralRetainedCleanup(FAutomationTestBase& In):T(&In){}
 bool Update()override{
  if(FPlatformTime::Seconds()>Deadline){T->AddError(TEXT("中央runner退出/Editor重试超时；保留真实失败，不计成功。"));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld:nullptr;auto* Resources=IGamePlatformEditorResources::Get();if(!Resources){T->AddError(TEXT("缺真实Editor资源模块"));return true;}
  if(Stage==0){if(!W||!W->HasBegunPlay())return false;auto* GI=W->GetGameInstance();auto* Catalog=GI?GI->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>():nullptr;
   if(!Catalog||!Catalog->FindScenarioDefinition(TEXT("BlueprintAutoLayout.settings.Manual"),Scenario))return false;Runner=W->GetSubsystem<UDivineBeastsDemoWorldSubsystem>();if(!Runner.IsValid()){T->AddError(TEXT("真实World缺中央runner"));return true;}
   BlueprintAutoLayoutManual::ReadSettings(Original);if(!T->TestTrue(TEXT("实际Catalog预检与中央BeginScenario"),Runner->BeginScenario(Scenario)))return true;Stage=1;return false;}
  if(Stage==1){auto* Live=Runner.Get();if(!Live){T->AddError(TEXT("中央runner准备时失效"));return true;}const auto Snap=Live->GetRunSnapshot();if(Snap.State==EDivineBeastsDemoRunState::Preparing)return false;if(!T->TestTrue(TEXT("中央runner实际运行"),Snap.State==EDivineBeastsDemoRunState::Running))return true;
   for(const auto& P:Scenario.Parameters){auto V=P.DefaultValue;if(P.ParameterId==TEXT("HorizontalSpacing"))V.IntegerValue=223;Values.Add(V);}T->TestTrue(TEXT("中央runner创建本Run原生图"),Live->RequestScenarioOperation(TEXT("CreateGraph"),Values));Stage=2;return false;}
  if(Stage==2){auto* Live=Runner.Get();if(!Live)return true;if(Live->GetRunSnapshot().bOperationPending)return false;T->TestTrue(TEXT("中央原生图创建成功"),Live->GetRunSnapshot().OperationResult.IsSuccess());T->TestTrue(TEXT("中央runner应用真实设置"),Live->RequestScenarioOperation(TEXT("ApplySettings"),Values));Stage=3;return false;}
  if(Stage==3){auto* Live=Runner.Get();if(!Live)return true;if(Live->GetRunSnapshot().bOperationPending)return false;T->TestTrue(TEXT("真实设置应用完成"),Live->GetRunSnapshot().OperationResult.IsSuccess());BlueprintAutoLayoutManual::ReadSettings(Written);T->TestEqual(TEXT("本Run写入真实值"),Written.HorizontalSpacing,223);
   External=Written;External.HorizontalSpacing=224;FString Error;T->TestTrue(TEXT("模拟独立工具后继设置"),BlueprintAutoLayoutManual::ApplySettings(External,Error));Lease=Resources->GetCleanupSnapshot().Lease;T->TestTrue(TEXT("捕获当前完整原租约"),Lease.IsValid());
   // 不调用Adapter.Stop或保存Adapter；真实EndPIE走WorldCleanup→Subsystem.Deinitialize→Adapter.Reset。
   GEditor->RequestEndPlayMap();Stage=4;return false;}
  if(Stage==4){if(W)return false;CollectGarbage(RF_NoFlags);T->TestFalse(TEXT("真实EndPIE后中央runner没有被测试或清理状态强持"),Runner.IsValid());const auto Pending=Resources->GetCleanupSnapshot();T->TestTrue(TEXT("真实runner销毁后Editor仍拥有完整失败清理"),Pending.bPending&&Pending.Lease==Lease);T->TestTrue(TEXT("准确冲突原因可见"),Pending.LastError.Contains(TEXT("SettingsConflict")));

   Runner.Reset();GEditor->RequestPlaySession(FRequestPlaySessionParams());Stage=5;return false;}
  if(Stage==5){if(!W||!W->HasBegunPlay())return false;auto* GI=W->GetGameInstance();auto* Catalog=GI?GI->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>():nullptr;if(!Catalog||!Catalog->FindScenarioDefinition(TEXT("BlueprintAutoLayout.settings.Manual"),Scenario))return false;Runner=W->GetSubsystem<UDivineBeastsDemoWorldSubsystem>();
   if(!Runner.IsValid()||!T->TestTrue(TEXT("新真实PIE中央runner在旧租约挂起时仍可进入既有恢复页"),Runner->BeginScenario(Scenario)))return true;Stage=6;return false;}
  if(Stage==6){auto* Live=Runner.Get();if(!Live)return true;const auto Snap=Live->GetRunSnapshot();if(Snap.State==EDivineBeastsDemoRunState::Preparing)return false;if(!T->TestTrue(TEXT("恢复页真正运行而非Prepare永久被旧租约阻塞"),Snap.State==EDivineBeastsDemoRunState::Running))return true;
   T->TestTrue(TEXT("恢复页显示完整旧挂起身份"),Snap.ObservationText.Contains(Lease.LeaseId.ToString())&&Snap.ObservationText.Contains(Lease.RunId.ToString())&&Snap.ObservationText.Contains(Lease.WorldId.ToString())&&Snap.ObservationText.Contains(TEXT("Generation=")));
   T->TestTrue(TEXT("恢复页当前Run与旧Run分离"),Snap.Token.RunId!=Lease.RunId);T->TestTrue(TEXT("恢复页中文观察操作可达"),Live->RequestScenarioOperation(TEXT("InspectPendingCleanup"),Values));T->TestTrue(TEXT("恢复页普通重试操作可达"),Live->RequestScenarioOperation(TEXT("RetryPendingCleanup"),Values));Stage=7;return false;}
  if(Stage==7){auto* Live=Runner.Get();if(!Live)return true;if(Live->GetRunSnapshot().bOperationPending)return false;T->TestFalse(TEXT("恢复页原CAS重试报告真实冲突"),Live->GetRunSnapshot().OperationResult.IsSuccess());BlueprintAutoLayoutManual::FSettingsSnapshot Current;BlueprintAutoLayoutManual::ReadSettings(Current);T->TestTrue(TEXT("页面普通重试完整保留外部后继"),Current==External);
   FString Error;auto Wrong=Lease;Wrong.Generation++;T->TestFalse(TEXT("伪完整身份不能处置原清理"),Resources->RetryCleanup(Wrong,EGamePlatformEditorCleanupPolicy::PreserveExternalChanges,Error));T->TestTrue(TEXT("恢复页明确保留外部设置按钮可达"),Live->RequestScenarioOperation(TEXT("PreserveExternalAndRetry"),Values));Stage=8;return false;}
  if(Stage==8){auto* Live=Runner.Get();if(!Live)return true;if(Live->GetRunSnapshot().bOperationPending)return false;T->TestTrue(TEXT("页面明确处置完成真实清理"),Live->GetRunSnapshot().OperationResult.IsSuccess());T->TestFalse(TEXT("成功后Editor不再保留失败状态"),Resources->GetCleanupSnapshot().bPending);T->TestFalse(TEXT("成功后原资源租约确实释放"),Resources->GetCleanupSnapshot().Lease.IsValid());BlueprintAutoLayoutManual::FSettingsSnapshot Current;BlueprintAutoLayoutManual::ReadSettings(Current);T->TestTrue(TEXT("冲突处置未写回旧设置"),Current==External);T->TestTrue(TEXT("恢复页Stop不操作其它所有者"),Live->RequestScenarioStop(FGamePlatformResult::Success()));Stage=9;return false;}
  if(Stage==9){auto* Live=Runner.Get();if(!Live)return true;if(Live->HasActiveScenario())return false;T->TestTrue(TEXT("处置后同页重新Prepare普通案例"),Live->BeginScenario(Scenario));Stage=10;return false;}
  if(Stage==10){auto* Live=Runner.Get();if(!Live)return true;const auto Snap=Live->GetRunSnapshot();if(Snap.State==EDivineBeastsDemoRunState::Preparing)return false;T->TestTrue(TEXT("恢复后普通真实功能可运行"),Snap.State==EDivineBeastsDemoRunState::Running);T->TestTrue(TEXT("新Run取得独立原生图功能而不是一直恢复模式"),Live->RequestScenarioOperation(TEXT("CreateGraph"),Values));Stage=11;return false;}
  if(Stage==11){auto* Live=Runner.Get();if(!Live)return true;if(Live->GetRunSnapshot().bOperationPending)return false;T->TestTrue(TEXT("恢复后的真实图创建完成"),Live->GetRunSnapshot().OperationResult.IsSuccess());Live->RequestScenarioStop(FGamePlatformResult::Success());Stage=12;return false;}
  if(Stage==12){auto* Live=Runner.Get();if(!Live)return true;const auto Snap=Live->GetRunSnapshot();if(Live->HasActiveScenario()){if(Snap.bCleanupPending&&Snap.State==EDivineBeastsDemoRunState::Failed)Live->RequestScenarioStop(FGamePlatformResult::Success());return false;}FString Error;T->TestTrue(TEXT("测试只恢复自己模拟的外部修改"),BlueprintAutoLayoutManual::RestoreSettings(Original,External,Error));Runner.Reset();return true;}
  return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditorCentralRunnerCleanup,"DivineBeasts.PluginDemos.Editor.PIE.CentralRunnerRetainedCleanup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEditorCentralRunnerCleanup::RunTest(const FString&){if(!GEditor||GEditor->PlayWorld){AddError(TEXT("要求空闲真实Editor；必须先原生作者合入T06 Catalog案例"));return false;}ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FEditorCentralRetainedCleanup(*this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif

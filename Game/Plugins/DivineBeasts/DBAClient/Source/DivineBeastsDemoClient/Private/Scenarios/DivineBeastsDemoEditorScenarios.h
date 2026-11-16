// T06开发人工页：项目私有Editor适配或平台中立租约；仅游戏线程，事件驱动。
// 只拥有本Run对象/完整租约，停止失败保留身份供重试；不操作用户图、账号或生产服务器。
#pragma once
#include "API/IDivineBeastsDemoScenarioAdapter.h"
class UDivineBeastsDemoCatalogSubsystem;
namespace DivineBeasts::Demo {
TArray<FName> EditorManualFeatures();
/** 作者元数据与执行器使用同一个封闭合同，来源类型仍属项目third_party_editor。 */
FDivineBeastsDemoScenarioDefinition EditorManualContract(FName Feature);
bool ValidateEditorManualContract(const FDivineBeastsDemoScenarioDefinition& Definition);
FGamePlatformResult ValidateEditorManualValues(const FDivineBeastsDemoScenarioDefinition& Definition,const TArray<FDivineBeastsDemoParameterValue>& Values);
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateEditorManualScenario(UWorld& World,FName Feature);
/** 九个工厂全部成功才发布；任一失败按完整句柄回滚。Client保留Unsupported适配，不链接Editor。 */
TArray<FDivineBeastsDemoAdapterRegistrationHandle> RegisterEditorManualScenarios(UDivineBeastsDemoCatalogSubsystem& Catalog);
}

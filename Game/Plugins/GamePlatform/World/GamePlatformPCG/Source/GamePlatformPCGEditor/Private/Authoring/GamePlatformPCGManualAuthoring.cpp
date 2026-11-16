#include "Authoring/GamePlatformPCGManualAuthoring.h"
#include "Authoring/GamePlatformPCGEditorLibrary.h"
#include "Actors/GamePlatformPCGActors.h"
#include "PCGGraph.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "Definitions/GamePlatformPCGEnvironmentDefinitions.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
namespace GamePlatformPCGManualAuthoring {
bool MakeRealizedPackage(FName Name,FString& Package,FString& Error){
 Package.Reset();const FString Text=Name.ToString();
 bool Valid=!Name.IsNone()&&!Text.IsEmpty()&&Text.Len()<=96;
 for(TCHAR C:Text)Valid&=(C>='A'&&C<='Z')||(C>='a'&&C<='z')||(C>='0'&&C<='9')||C=='_';
 if(!Valid){Error=TEXT("输出名必须为非空ASCII字母数字下划线资产标识；拒绝路径和保留空名。" );return false;}
 Package=TEXT("/Game/Development/Foundation/PCG/Realized/")+Text;return true;
}
FGamePlatformResult CreateRealizedGraph(IGamePlatformDataService& Data,const FGamePlatformDataLease& Lease,
 const FSoftObjectPath& ProfilePath,const FSoftObjectPath& MeshSetPath,FName Name,TFunctionRef<bool()> IsCurrent,FString& SavedPath){
 check(IsInGameThread());SavedPath.Reset();FString Package,Error;
 if(!MakeRealizedPackage(Name,Package,Error))return FGamePlatformResult::Failure(TEXT("PCG.Manual.OutputScope"),Error);
 if(!IsCurrent())return FGamePlatformResult::Cancelled(TEXT("作者提交前来源已取消"));
 if(!Lease.IsValid()||Data.GetLeaseState(Lease)!=EGamePlatformDataRequestState::Succeeded||Lease.Bundles!=TArray<FName>{TEXT("PCGGeneration")})
  return FGamePlatformResult::Failure(TEXT("PCG.Manual.DefinitionLease"),TEXT("必须持有本Run成功的Definition及PCGGeneration完整递归租约"));
 const auto* Profile=Cast<UGamePlatformPCGProfileDefinition>(Data.GetLoadedDefinition(Lease));
 auto* MeshSet=Cast<UGamePlatformPCGMeshSetDefinition>(MeshSetPath.ResolveObject());
 auto& Manager=UAssetManager::Get(); // 只读canonical索引，不加载/另建管理器。
 if(!Profile||ProfilePath.ResolveObject()!=Profile||Manager.GetPrimaryAssetPath(Lease.DefinitionId)!=ProfilePath||
  Profile->GetPrimaryAssetId()!=Lease.DefinitionId||!MeshSet||MeshSetPath.GetSubPathString().Len()||
  MeshSet->GetPrimaryAssetId()!=Profile->MeshSetDefinitionId||!Profile->RequiredDefinitions.Contains(Profile->MeshSetDefinitionId)||
  Manager.GetPrimaryAssetPath(Profile->MeshSetDefinitionId)!=MeshSetPath||!Profile->GraphReference.Get()||MeshSet->Entries.IsEmpty())
  return FGamePlatformResult::Failure(TEXT("PCG.Manual.CanonicalDependencies"),TEXT("Profile/MeshSet canonical身份、递归依赖、已加载图不匹配"));
 for(const auto& Entry:MeshSet->Entries)if(!Entry.Mesh.Get())return FGamePlatformResult::Failure(TEXT("PCG.Manual.MeshNotLeased"),TEXT("递归PCGGeneration租约尚无可读Mesh"));
 if(FindPackage(nullptr,*Package)||FPackageName::DoesPackageExist(Package))
  return FGamePlatformResult::Failure(TEXT("PCG.Manual.OutputOccupied"),TEXT("输出包已存在或被占用；拒绝覆盖"));
 // 唯一原作者内部只克隆Profile验证并保存新图；源Profile/模板始终只读。
 auto* Graph=UGamePlatformPCGEditorLibrary::CreateDevelopmentRealizedGraphAsset(Package,const_cast<UGamePlatformPCGProfileDefinition*>(Profile),MeshSet,Error);
 if(Graph)SavedPath=Graph->GetPathName();
 if(!IsCurrent())return FGamePlatformResult::Cancelled(TEXT("作者保存栈期间取消；已保存产物保留，Stop不删除，不写后继Run"));
 return Graph?FGamePlatformResult::Success():FGamePlatformResult::Failure(TEXT("PCG.Manual.CreateFailed"),Error);
}
bool CreateFoundations(FString& E){return UGamePlatformPCGEditorLibrary::CreateFoundationAssets(E);}
bool CreateManualPlacements(FString& E){return UGamePlatformPCGEditorLibrary::CreatePCGPlacementBlueprints(TEXT("/Game/Development/PluginDemos/PCG/Blueprints/"),E);}
bool ValidateProfile(UGamePlatformPCGProfileDefinition* P,FString& E){return UGamePlatformPCGEditorLibrary::ValidateProfileContract(P,E);}
bool InspectSource(UGamePlatformPCGProfileDefinition* P,FString& F,TArray<FString>& D,FString& E){return UGamePlatformPCGEditorLibrary::InspectProfileSource(P,F,D,E);}
bool ConfigureOwnedSpatialMasks(UPCGGraph* G,AGamePlatformPCGWorldDirector* D,int32 P,FString& E){
 if(!G||!D||!G->HasAnyFlags(RF_Transient)||!D->HasAnyFlags(RF_Transient)||!D->GetWorld()||D->GetWorld()->bIsTearingDown){E=TEXT("需要当前World本Run的瞬态图和Director；拒绝写入源图或正式地图。");return false;}
 TArray<FGamePlatformPCGSpatialMask> Masks;if(!D->CollectSpatialMasks(Masks,E))return false;
 if(Masks.IsEmpty()){E=TEXT("本Director未登记真实空间参与者，缺宿主不能把空掩码称为烘焙完成。");return false;}
 return UGamePlatformPCGEditorLibrary::ConfigureRealizedGraphSpatialMasks(G,D,P,{},E);
}
}

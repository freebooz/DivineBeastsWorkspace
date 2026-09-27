#include "Interfaces/IGamePlatformDataService.h"
#include "Loading/GamePlatformAssetManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
/** 真实内存资产+注册表+AssetManager扫描，最终调用运行期公开服务；不调用编辑器图验证器。 */
class FRuntimeDependencyFailuresCommand final : public IAutomationLatentCommand
{
public:
    explicit FRuntimeDependencyFailuresCommand(FAutomationTestBase* InTest)
        : Test(InTest), StartedSeconds(FPlatformTime::Seconds()), RunText(FGuid::NewGuid().ToString(EGuidFormats::Digits))
    {
        RootPath = TEXT("/Game/__GamePlatformDataRuntimeTests/") + RunText;
    }
    virtual ~FRuntimeDependencyFailuresCommand() override { Alive.Reset(); Cleanup(); }
    virtual bool Update() override
    {
        if (FPlatformTime::Seconds() - StartedSeconds > 30.0)
        { Test->AddError(TEXT("运行期递归依赖失败测试超时。")); Cleanup(); return true; }
        auto* Manager = Cast<UGamePlatformAssetManager>(UAssetManager::GetIfInitialized());
        if (!Manager || !GIsEditor || IsRunningCookCommandlet())
        { Test->AddError(TEXT("该内存源扫描测试要求已配置的编辑器AssetManager，不能在烘焙或非编辑器中伪通过。")); return true; }
        if (!bHasStarted)
        {
            IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
            Registry.SearchAllAssets(true); Registry.WaitForCompletion();
            if (!Manager->GetPrimaryAssetTypeInfo(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), TypeInfo) ||
                TypeInfo.bHasBlueprintClasses || !TypeInfo.AssetBaseClassLoaded ||
                !UGamePlatformDefinitionBase::StaticClass()->IsChildOf(TypeInfo.AssetBaseClassLoaded.Get()))
            { Test->AddError(TEXT("正式GamePlatformDefinition扫描基线缺失或不兼容。")); return true; }
            auto* Shared = AddDefinition(TEXT("Shared"));
            auto* MissingRoot = AddDefinition(TEXT("MissingRoot"));
            auto* CycleRoot = AddDefinition(TEXT("CycleRoot"));
            auto* CycleChild = AddDefinition(TEXT("CycleChild"));
            SharedId = Shared->GetPrimaryAssetId();
            MissingRoot->RequiredDefinitions = {SharedId, FPrimaryAssetId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(),
                FName(*(TEXT("test.run") + RunText + TEXT(".absent@1"))))};
            CycleRoot->RequiredDefinitions = {SharedId, CycleChild->GetPrimaryAssetId()};
            CycleChild->RequiredDefinitions = {CycleRoot->GetPrimaryAssetId()};
            // UE5.8 SearchAssetRegistryPaths在编辑器非Cook中bIncludeOnlyOnDiskAssets=false，实际会查内存资产。
            bHasScanned = true;
            const int32 Found = Manager->ScanPathsForPrimaryAssets(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), {RootPath},
                TypeInfo.AssetBaseClassLoaded.Get(), TypeInfo.bHasBlueprintClasses, TypeInfo.bIsEditorOnly, false);
            if (!Test->TestEqual(TEXT("真实扫描发现四个内存源定义"), Found, 4)) { Cleanup(); return true; }
            for (const auto& Definition : Definitions)
                if (!Test->TestEqual(TEXT("真实主资产映射对应隔离对象"), Manager->GetPrimaryAssetPath(Definition->GetPrimaryAssetId()), FSoftObjectPath(Definition.Get())))
                { Cleanup(); return true; }
            Instance.Reset(NewObject<UGameInstance>(GEngine));
            Instance->InitializeStandalone(FName(*(TEXT("DataDependencyTest_") + RunText)));
            bHasInitializedInstance = true;
            Service = IGamePlatformDataService::Get(*Instance);
            if (!Service) { Test->AddError(TEXT("实际GI初始化后没有公开数据门面。")); Cleanup(); return true; }
            FGamePlatformResult Accepted;
            const TWeakPtr<int32> WeakAlive(Alive);
            Survivor = Service->AcquireDefinition(SharedId, UGamePlatformDefinitionBase::StaticClass(), {TEXT("Core"), TEXT("Server")},
                EGamePlatformDataLifetime::Instance, Instance.Get(), [this, WeakAlive](const auto&, const auto& Result)
                { if (WeakAlive.IsValid()) { ++SurvivorCount; SurvivorResult = Result; } }, Accepted);
            Test->TestTrue(TEXT("独立共享依赖租约接纳"), Accepted.IsSuccess());
            Missing = Service->AcquireDefinition(MissingRoot->GetPrimaryAssetId(), UGamePlatformDefinitionBase::StaticClass(), {TEXT("Core"), TEXT("UI")},
                EGamePlatformDataLifetime::Instance, Instance.Get(), [this, WeakAlive](const auto&, const auto& Result)
                { if (WeakAlive.IsValid()) { ++MissingCount; MissingResult = Result; } }, Accepted);
            Test->TestTrue(TEXT("缺失图先作为异步请求接纳"), Accepted.IsSuccess());
            Cycle = Service->AcquireDefinition(CycleRoot->GetPrimaryAssetId(), UGamePlatformDefinitionBase::StaticClass(), {TEXT("Core"), TEXT("UI")},
                EGamePlatformDataLifetime::Instance, Instance.Get(), [this, WeakAlive](const auto&, const auto& Result)
                { if (WeakAlive.IsValid()) { ++CycleCount; CycleResult = Result; } }, Accepted);
            Test->TestTrue(TEXT("循环图先作为异步请求接纳"), Accepted.IsSuccess());
            bHasStarted = true;
            return false;
        }
        if (MissingCount == 0 || CycleCount == 0 || SurvivorCount == 0) return false;
        if (const auto Pending = Manager->GetPrimaryAssetHandle(SharedId); Pending && !Pending->HasLoadCompleted()) return false;
        Test->TestTrue(TEXT("独立共享依赖请求仍成功"), SurvivorResult.IsSuccess());
        Test->TestEqual(TEXT("运行期服务检出缺失依赖"), MissingResult.Code, FName(TEXT("MissingDefinition")));
        Test->TestEqual(TEXT("运行期服务检出真实循环"), CycleResult.Code, FName(TEXT("DependencyCycle")));
        Test->TestEqual(TEXT("缺失请求只完成一次"), MissingCount, 1);
        Test->TestEqual(TEXT("循环请求只完成一次"), CycleCount, 1);
        Test->TestNotNull(TEXT("失败回滚不能卸掉其他租约共享定义"), Service->GetLoadedDefinition(Survivor));
        Test->TestNull(TEXT("失败根不能读取"), Service->GetLoadedDefinition(Missing));
        Test->TestNull(TEXT("循环根不能读取"), Service->GetLoadedDefinition(Cycle));
        TArray<FName> Bundles;
        Manager->GetPrimaryAssetHandle(SharedId, true, &Bundles);
        Test->TestTrue(TEXT("失败回滚保留独立Server需求"), Bundles.Contains(TEXT("Core")) && Bundles.Contains(TEXT("Server")));
        Test->TestFalse(TEXT("两个失败图撤销自己的UI需求"), Bundles.Contains(TEXT("UI")));
        Test->TestEqual(TEXT("无遗留等待请求"), Service->GetDiagnostics().PendingRequests, 0);
        Test->TestEqual(TEXT("只有独立成功租约仍活跃"), Service->GetDiagnostics().ActiveLeases, 1);
        Service->ReleaseDefinition(Missing); Service->ReleaseDefinition(Cycle); Service->ReleaseDefinition(Survivor);
        Cleanup();
        return true;
    }
private:
    UGamePlatformDefinitionBase* AddDefinition(const FString& Name)
    {
        UPackage* Package = CreatePackage(*(RootPath + TEXT("/") + Name));
        Package->SetPackageFlags(PKG_Transient);
        auto* Definition = NewObject<UGamePlatformDefinitionBase>(Package, *Name, RF_Public | RF_Standalone);
        const FString LogicalText = TEXT("test.run") + RunText + TEXT(".") + Name.ToLower() + TEXT("@1");
        if (!FGamePlatformId::TryParse(LogicalText, Definition->LogicalId)) Test->AddError(TEXT("隔离夹具身份生成失败。"));
        Definitions.Emplace(Definition);
        FAssetRegistryModule::AssetCreated(Definition);
        return Definition;
    }
    void Cleanup()
    {
        if (bHasInitializedInstance)
        {
            UWorld* World = Instance->GetWorld();
            if (World) World->DestroyWorld(false);
            // 真实子系统关闭期间仍可能读取GI上下文，必须在Shutdown之后才移除引擎上下文。
            Instance->Shutdown(); bHasInitializedInstance = false; Service = nullptr;
            if (World) GEngine->DestroyWorldContext(World);
        }
        if (auto* Manager = Cast<UGamePlatformAssetManager>(UAssetManager::GetIfInitialized()); Manager && bHasScanned)
        {
            TArray<FPrimaryAssetId> Ids;
            for (const auto& Definition : Definitions) Ids.Add(Definition->GetPrimaryAssetId());
            Manager->UnloadPrimaryAssets(Ids);
            // 根路径由本测试GUID生成且包含尾斜杠，只撤销本轮内存包，不移除全局定义类型。
            Manager->RemovePrimaryAssetsForTypeInMountPoint(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), RootPath + TEXT("/"));
            Manager->RemoveScanPathsForPrimaryAssets(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), {RootPath},
                TypeInfo.AssetBaseClassLoaded.Get(), TypeInfo.bHasBlueprintClasses, TypeInfo.bIsEditorOnly);
            bHasScanned = false;
        }
        for (const auto& Definition : Definitions)
        {
            FAssetRegistryModule::AssetDeleted(Definition.Get());
            Definition->ClearFlags(RF_Public | RF_Standalone); Definition->MarkAsGarbage();
        }
        Definitions.Empty();
    }
    FAutomationTestBase* Test;
    double StartedSeconds;
    FString RunText, RootPath;
    FPrimaryAssetTypeInfo TypeInfo;
    FPrimaryAssetId SharedId;
    TSharedPtr<int32> Alive = MakeShared<int32>(0);
    TArray<TStrongObjectPtr<UGamePlatformDefinitionBase>> Definitions;
    TStrongObjectPtr<UGameInstance> Instance;
    IGamePlatformDataService* Service = nullptr;
    FGamePlatformDataLease Survivor, Missing, Cycle;
    FGamePlatformResult SurvivorResult, MissingResult, CycleResult;
    int32 SurvivorCount = 0, MissingCount = 0, CycleCount = 0;
    bool bHasStarted = false, bHasScanned = false, bHasInitializedInstance = false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformRuntimeDependencyTest, "GamePlatform.Data.Runtime.RecursiveFailureRollback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformRuntimeDependencyTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FRuntimeDependencyFailuresCommand(this));
    return true;
}
#endif

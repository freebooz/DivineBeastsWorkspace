// 平台层私有索引的真实AssetRegistry编辑器回归：复用现有GUID内存包、强引用与AssetCreated/Deleted夹具模式。
// 只修改本测试创建的临时定义；不保存磁盘资产、不启动世界或更换AssetManager，结束明确撤销注册源。
#include "Validation/GamePlatformDefinitionSourceIndex.h"
#include "Validation/GamePlatformDefinitionValidation.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/DataAsset.h"
#include "Misc/AutomationTest.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace
{
using GamePlatform::Data::FDefinitionSourceIndex;

/** 真实UObject/真实注册表事件的独占内存夹具；不依赖工程正式定义与主资产扫描配置。 */
struct FDefinitionSourceRegistryFixture
{
    IAssetRegistry& Registry;
    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString RootPath = TEXT("/Game/__GamePlatformDataIndexTests/") + Suffix;
    TArray<TStrongObjectPtr<UGamePlatformDefinitionBase>> Definitions;
    TSet<UGamePlatformDefinitionBase*> Removed;

    explicit FDefinitionSourceRegistryFixture(IAssetRegistry& InRegistry) : Registry(InRegistry) {}
    ~FDefinitionSourceRegistryFixture() { Cleanup(); }

    /** GUID保证与正式内容及重复运行隔离；名称长度保持在GamePlatformId合同内。 */
    FGamePlatformId Id(const TCHAR* Name) const
    {
        FGamePlatformId Result;
        FGamePlatformId::TryParse(TEXT("test.index_") + Suffix + TEXT("_") + Name + TEXT("@1"), Result);
        return Result;
    }

    /** 只创建新建未保存包；AssetCreated同步触发真实注册表新增事件。 */
    UGamePlatformDefinitionBase* Add(const FGamePlatformId& LogicalId)
    {
        UPackage* Package = CreatePackage(*(RootPath + FString::Printf(TEXT("/Asset%d"), Definitions.Num())));
        Package->SetPackageFlags(PKG_NewlyCreated);
        auto* Definition = NewObject<UGamePlatformDefinitionBase>(Package, TEXT("Definition"), RF_Public | RF_Standalone);
        Definition->LogicalId = LogicalId;
        Definitions.Emplace(Definition);
        FAssetRegistryModule::AssetCreated(Definition);
        return Definition;
    }

    /** 通过UE5.8正式完整标签更新入口触发Added/Updated，不手工广播伪造事件。 */
    void UpdateIdentity(UGamePlatformDefinitionBase* Definition, const FGamePlatformId& NewId)
    {
        Definition->LogicalId = NewId;
        Registry.AssetUpdateTags(Definition, EAssetRegistryTagsCaller::FullUpdate);
    }

    /** 重命名真实临时对象后按引擎约定通知原路径；不创建Redirector或修改其他资产。 */
    bool Rename(UGamePlatformDefinitionBase* Definition)
    {
        const FString OldPath = Definition->GetPathName();
        if (!Definition->Rename(TEXT("RenamedDefinition"), nullptr, REN_DontCreateRedirectors | REN_NonTransactional))
            return false;
        Registry.AssetRenamed(Definition, OldPath);
        return true;
    }

    /** 幂等撤销本夹具源，清除资产保留标记并交给引擎GC；不手动销毁用户包。 */
    void Remove(UGamePlatformDefinitionBase* Definition)
    {
        if (!Definition || Removed.Contains(Definition)) return;
        FAssetRegistryModule::AssetDeleted(Definition);
        Definition->ClearFlags(RF_Public | RF_Standalone);
        Definition->MarkAsGarbage();
        Removed.Add(Definition);
    }

    void Cleanup()
    {
        for (auto& Definition : Definitions) Remove(Definition.Get());
        // Registry对已删除内存包使用EmptyPackage语义隐藏旧标签；不把其内部磁盘缓存当作可见源。
        Registry.RemovePath(RootPath);
    }
};

FPrimaryAssetId RegistryTestId(const FGamePlatformId& LogicalId)
{
    return FPrimaryAssetId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), FName(*LogicalId.ToString()));
}

/** 只读委托所有者检查，确认Shutdown真正解除八个事件，而不只是在遗留回调中忽略消息。 */
bool HasIndexRegistryBindings(IAssetRegistry& Registry, const FDefinitionSourceIndex& Index)
{
    return Registry.OnAssetAdded().IsBoundToObject(&Index) ||
        Registry.OnAssetRemoved().IsBoundToObject(&Index) ||
        Registry.OnAssetUpdated().IsBoundToObject(&Index) ||
        Registry.OnAssetUpdatedOnDisk().IsBoundToObject(&Index) ||
        Registry.OnAssetRenamed().IsBoundToObject(&Index) ||
        Registry.OnFilesLoaded().IsBoundToObject(&Index) ||
        Registry.OnScanStarted().IsBoundToObject(&Index) ||
        Registry.OnKnownGathersComplete().IsBoundToObject(&Index);
}

/** 编辑器回归可等待真实发现结束；生产索引本身不执行SearchAllAssets或同步等待。 */
IAssetRegistry& ReadyRegistry()
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.SearchAllAssets(true);
    Registry.WaitForCompletion();
    return Registry;
}
}

// 实际资产事件、公开入口及独立实例并行验证；计数只观测本测试实例，避免其他测试污染。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformDefinitionIndexRegistryEventsTest,
    "GamePlatform.Data.Editor.SourceIndexRegistryEvents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformDefinitionIndexRegistryEventsTest::RunTest(const FString& Parameters)
{
    IAssetRegistry& Registry = ReadyRegistry();
    if (!TestFalse(TEXT("实际Registry发现已完成"), Registry.IsLoadingAssets() || Registry.IsGathering())) return false;
    FDefinitionSourceRegistryFixture Fixture(Registry);
    const FGamePlatformId FirstId = Fixture.Id(TEXT("first"));
    const FGamePlatformId SecondId = Fixture.Id(TEXT("second"));
    const FGamePlatformId FinalId = Fixture.Id(TEXT("final"));
    const FGamePlatformId CandidateId = Fixture.Id(TEXT("candidate"));
    auto* First = Fixture.Add(FirstId);
    FDefinitionSourceIndex Index;
    Index.Startup(Registry);
    FSoftObjectPath Path;
    TestEqual(TEXT("Startup不扫描"), Index.GetDiagnostics().FullScanCount, uint64(0));
    TestTrue(TEXT("真实源冷查询"), Index.Resolve(RegistryTestId(FirstId), Path).IsSuccess());
    TestEqual(TEXT("真实物理路径"), Path, FSoftObjectPath(First));
    TestTrue(TEXT("公开入口读取模块索引"), ResolveUniqueGamePlatformDefinitionSource(RegistryTestId(FirstId), Path).IsSuccess());
    for (int32 Repeat = 0; Repeat < 64; ++Repeat)
        TestTrue(TEXT("真实Registry暖查询"), Index.Resolve(RegistryTestId(FirstId), Path).IsSuccess());
    TestEqual(TEXT("64次查询仍只有一次全扫描"), Index.GetDiagnostics().FullScanCount, uint64(1));

    auto* Duplicate = Fixture.Add(FirstId);
    TestEqual(TEXT("AssetCreated使冲突立即可见"), Index.Resolve(RegistryTestId(FirstId), Path).Code, FName(TEXT("DuplicateLogicalId")));
    TestEqual(TEXT("公开入口同样拒绝重复"), ResolveUniqueGamePlatformDefinitionSource(RegistryTestId(FirstId), Path).Code, FName(TEXT("DuplicateLogicalId")));
    Fixture.Remove(Duplicate);
    TestTrue(TEXT("AssetDeleted解除重复"), Index.Resolve(RegistryTestId(FirstId), Path).IsSuccess());

    Fixture.UpdateIdentity(First, SecondId);
    TestEqual(TEXT("完整标签更新撤销旧ID"), Index.Resolve(RegistryTestId(FirstId), Path).Code, FName(TEXT("MissingDefinition")));
    TestTrue(TEXT("完整标签更新加入新ID"), Index.Resolve(RegistryTestId(SecondId), Path).IsSuccess());
    // 第一次AssetUpdateTags可能创建缓存记录；第二次确实更新同一路径的既有标签。
    Fixture.UpdateIdentity(First, FinalId);
    TestEqual(TEXT("再次标签更新无旧ID残留"), Index.Resolve(RegistryTestId(SecondId), Path).Code, FName(TEXT("MissingDefinition")));
    TestTrue(TEXT("再次更新后可用"), Index.Resolve(RegistryTestId(FinalId), Path).IsSuccess());
    TestEqual(TEXT("公开入口不会继续返回旧ID"), ResolveUniqueGamePlatformDefinitionSource(RegistryTestId(SecondId), Path).Code, FName(TEXT("MissingDefinition")));

    // 未保存候选只覆盖它自己，不发送标签通知，以验证独立的编辑候选合同。
    First->LogicalId = CandidateId;
    TestEqual(TEXT("候选替换旧身份"), Index.Resolve(RegistryTestId(FinalId), Path, First).Code, FName(TEXT("MissingDefinition")));
    TestTrue(TEXT("未保存候选新身份"), Index.Resolve(RegistryTestId(CandidateId), Path, First).IsSuccess());
    TestTrue(TEXT("公开入口候选新身份"), ResolveUniqueGamePlatformDefinitionSource(RegistryTestId(CandidateId), Path, First).IsSuccess());
    TestTrue(TEXT("候选未污染无候选快照"), Index.Resolve(RegistryTestId(FinalId), Path).IsSuccess());
    First->LogicalId = FinalId;

    const FSoftObjectPath OldPath(First);
    if (!TestTrue(TEXT("真实UObject重命名成功"), Fixture.Rename(First))) return false;
    TestTrue(TEXT("AssetRenamed后源仍唯一"), Index.Resolve(RegistryTestId(FinalId), Path).IsSuccess());
    TestEqual(TEXT("AssetRenamed返回新路径"), Path, FSoftObjectPath(First));
    TestNotEqual(TEXT("不再返回旧路径"), Path, OldPath);
    TestTrue(TEXT("公开入口重命名后成功"), ResolveUniqueGamePlatformDefinitionSource(RegistryTestId(FinalId), Path).IsSuccess());
    TestEqual(TEXT("公开入口也返回新路径"), Path, FSoftObjectPath(First));

    // 直接给私有索引一个有相同LogicalId的非定义载荷；不污染真实Registry或广播给其他插件。
    FAssetDataTagMap ForgedTags;
    ForgedTags.Add(UGamePlatformPrimaryDataAsset::LogicalIdTag(), FinalId.ToString());
    const FAssetData Unrelated(FName(*(Fixture.RootPath + TEXT("/Unrelated"))), FName(*Fixture.RootPath),
        FName(TEXT("Unrelated")), UDataAsset::StaticClass()->GetClassPathName(), MoveTemp(ForgedTags));
    Index.OnAssetAddedOrUpdated(Unrelated);
    TestTrue(TEXT("生产适配拒绝非定义同名标签"), Index.Resolve(RegistryTestId(FinalId), Path).IsSuccess());
    FAssetDataTagMap UnknownTags;
    UnknownTags.Add(UGamePlatformPrimaryDataAsset::LogicalIdTag(), FinalId.ToString());
    const FAssetData UnknownClass(FName(*(Fixture.RootPath + TEXT("/Unknown"))), FName(*Fixture.RootPath),
        FName(TEXT("Unknown")), FTopLevelAssetPath(TEXT("/Script/NoSuchDataIndexModule.NoSuchDefinition")), MoveTemp(UnknownTags));
    Index.OnAssetAddedOrUpdated(UnknownClass);
    TestTrue(TEXT("未加载未知类不被标签冒充且不触发加载"), Index.Resolve(RegistryTestId(FinalId), Path).IsSuccess());
    TestEqual(TEXT("全部普通资产事件均增量完成"), Index.GetDiagnostics().FullScanCount, uint64(1));

    Fixture.Cleanup();
    TestEqual(TEXT("删除真实源后立即缺失"), Index.Resolve(RegistryTestId(FinalId), Path).Code, FName(TEXT("MissingDefinition")));
    TestEqual(TEXT("公开入口删除后立即缺失"), ResolveUniqueGamePlatformDefinitionSource(RegistryTestId(FinalId), Path).Code, FName(TEXT("MissingDefinition")));
    Index.Invalidate();
    TestEqual(TEXT("重新枚举也不会复活已删除临时源"), Index.Resolve(RegistryTestId(FinalId), Path).Code, FName(TEXT("MissingDefinition")));
    TestEqual(TEXT("清理后只因显式失效增加一次扫描"), Index.GetDiagnostics().FullScanCount, uint64(2));
    TArray<FAssetData> Remaining;
    Registry.GetAssetsByPath(FName(*Fixture.RootPath), Remaining, true, false);
    TestEqual(TEXT("临时根没有可见资产遗留"), Remaining.Num(), 0);
    return true;
}

// 核对真实委托解绑与关闭后新资产事件；不卸载全局Data模块，不打断其他PIE/加载消费者。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformDefinitionIndexRegistryShutdownTest,
    "GamePlatform.Data.Editor.SourceIndexRegistryShutdown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformDefinitionIndexRegistryShutdownTest::RunTest(const FString& Parameters)
{
    IAssetRegistry& Registry = ReadyRegistry();
    if (!TestFalse(TEXT("实际Registry发现已完成"), Registry.IsLoadingAssets() || Registry.IsGathering())) return false;
    FDefinitionSourceRegistryFixture Fixture(Registry);
    const FGamePlatformId FirstId = Fixture.Id(TEXT("first"));
    const FGamePlatformId NextId = Fixture.Id(TEXT("next"));
    auto* First = Fixture.Add(FirstId);
    FDefinitionSourceIndex Index;
    Index.Startup(Registry);
    TestTrue(TEXT("实际委托已经绑定"), HasIndexRegistryBindings(Registry, Index));
    FSoftObjectPath Path;
    TestTrue(TEXT("关闭前冷查询可用"), Index.Resolve(RegistryTestId(FirstId), Path).IsSuccess());
    Index.Invalidate(); // 尚未重建的请求在Shutdown时被取消，不能残留自动重建任务。
    Index.Shutdown();
    TestFalse(TEXT("八个实际Registry委托全部解绑"), HasIndexRegistryBindings(Registry, Index));
    const auto Stopped = Index.GetDiagnostics();
    auto* Later = Fixture.Add(NextId);
    Fixture.UpdateIdentity(First, NextId);
    TestTrue(TEXT("关闭后真实重命名仍正常"), Fixture.Rename(First));
    Fixture.Remove(Later);
    TestEqual(TEXT("关闭实例不能被资产事件复活"), Index.GetDiagnostics().AssetEventCount, Stopped.AssetEventCount);
    TestEqual(TEXT("关闭实例解析未就绪"), Index.Resolve(RegistryTestId(NextId), Path).Code, FName(TEXT("AssetRegistryNotReady")));
    TestEqual(TEXT("关闭后没有隐式全扫描"), Index.GetDiagnostics().FullScanCount, Stopped.FullScanCount);

    Index.Startup(Registry);
    TestTrue(TEXT("显式重新启动重绑委托"), HasIndexRegistryBindings(Registry, Index));
    TestTrue(TEXT("重启从当前真实源冷建"), Index.Resolve(RegistryTestId(NextId), Path).IsSuccess());
    TestEqual(TEXT("重启只做一次枚举"), Index.GetDiagnostics().FullScanCount, uint64(1));
    Fixture.Cleanup();
    Index.Shutdown();
    TestFalse(TEXT("最终清理后无委托"), HasIndexRegistryBindings(Registry, Index));
    TestEqual(TEXT("最终清理后无私有源"), Index.GetDiagnostics().SourceCount, 0);
    return true;
}
#endif

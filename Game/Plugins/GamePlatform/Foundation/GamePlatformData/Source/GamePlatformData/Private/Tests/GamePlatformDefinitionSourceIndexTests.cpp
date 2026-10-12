// 平台层私有源索引行为回归：可控元数据/发现时序调用真实生产索引，不启动世界、不登记资产。
// 枚举计数直接观测源访问与快照发布；这些测试不替代实际Registry事件、Cook或PIE证据。
#include "Validation/GamePlatformDefinitionSourceIndex.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Engine/DataAsset.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
using GamePlatform::Data::FDefinitionSourceIndex;
using GamePlatform::Data::FDefinitionSourceIndexAccess;

/** 只供独立算法回归使用的源元数据，不写磁盘、不注入全局AssetRegistry。 */
FAssetData MakeIndexTestAsset(const FString& ObjectPath, const FString& LogicalId,
    FTopLevelAssetPath ClassPath = UGamePlatformDefinitionBase::StaticClass()->GetClassPathName())
{
    FAssetDataTagMap Tags;
    Tags.Add(UGamePlatformPrimaryDataAsset::LogicalIdTag(), LogicalId);
    const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
    return FAssetData(FStringView(PackageName), FStringView(ObjectPath), ClassPath, MoveTemp(Tags));
}

FPrimaryAssetId IndexTestId(const TCHAR* LogicalId)
{
    return FPrimaryAssetId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), FName(LogicalId));
}

/** 测试仅替换发现/枚举输入，所有查询、失效、反向删除与取消逻辑来自生产类。 */
struct FIndexTestSource
{
    bool bDiscovering = false;
    int32 EnumerationCalls = 0;
    TArray<FAssetData> Assets;
    TFunction<void()> DuringEnumeration;

    FDefinitionSourceIndexAccess MakeAccess()
    {
        FDefinitionSourceIndexAccess Access;
        Access.IsDiscovering = [this]() { return bDiscovering; };
        Access.EnumerateDefinitions = [this](TArray<FAssetData>& OutAssets)
        {
            ++EnumerationCalls;
            OutAssets = Assets;
            if (DuringEnumeration) DuringEnumeration();
        };
        Access.IsDefinitionAsset = [](const FAssetData& Asset)
        {
            // 独立夹具只用已加载原生类；未加载类过滤另外由真实Registry适配回归验证。
            return Asset.IsInstanceOf<UGamePlatformPrimaryDataAsset>(EResolveClass::No);
        };
        return Access;
    }
};
}

// 首次发现时禁止扫描；冷建一次后，无论命中/缺失/重复查询都只读索引。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformDefinitionIndexColdWarmTest,
    "GamePlatform.Data.Index.ColdWarmRepeatedQueries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformDefinitionIndexColdWarmTest::RunTest(const FString& Parameters)
{
    FIndexTestSource Source;
    const FAssetData Asset = MakeIndexTestAsset(TEXT("/Game/__DataIndex/A.A"), TEXT("test.first@1"));
    Source.Assets = {Asset, Asset}; // 重复同一物理记录不构成两个定义。
    Source.bDiscovering = true;
    FDefinitionSourceIndex Index;
    Index.Startup(Source.MakeAccess());
    FSoftObjectPath Path(TEXT("/Game/Stale.Stale"));
    TestEqual(TEXT("发现中明确未就绪"), Index.Resolve(IndexTestId(TEXT("test.first@1")), Path).Code, FName(TEXT("AssetRegistryNotReady")));
    TestTrue(TEXT("失败清除输出路径"), Path.IsNull());
    TestEqual(TEXT("发现中不枚举"), Source.EnumerationCalls, 0);

    Source.bDiscovering = false;
    TestTrue(TEXT("冷建唯一源"), Index.Resolve(IndexTestId(TEXT("test.first@1")), Path).IsSuccess());
    TestEqual(TEXT("返回物理路径"), Path, Asset.GetSoftObjectPath());
    const auto Warm = Index.GetDiagnostics();
    TestEqual(TEXT("冷建枚举一次"), Warm.FullScanCount, uint64(1));
    TestEqual(TEXT("冷建发布一次"), Warm.RebuildCount, uint64(1));
    TestEqual(TEXT("相同路径去重"), Warm.SourceCount, 1);
    for (int32 Repeat = 0; Repeat < 128; ++Repeat)
        TestTrue(TEXT("普通重复查询持续成功"), Index.Resolve(IndexTestId(TEXT("test.first@1")), Path).IsSuccess());
    TestEqual(TEXT("查询计数增加128"), Index.GetDiagnostics().QueryCount, Warm.QueryCount + 128);
    TestEqual(TEXT("128次暖查询无新增扫描"), Source.EnumerationCalls, 1);
    TestEqual(TEXT("缺失保持错误码"), Index.Resolve(IndexTestId(TEXT("test.missing@1")), Path).Code, FName(TEXT("MissingDefinition")));
    TestEqual(TEXT("非法ID保持错误码"), Index.Resolve(FPrimaryAssetId(), Path).Code, FName(TEXT("InvalidDefinitionId")));
    TestTrue(TEXT("所有失败输出为空"), Path.IsNull());
    TestEqual(TEXT("负查询也不全扫"), Source.EnumerationCalls, 1);
    return true;
}

// 每次事件验证可见行为与枚举次数，尤其覆盖旧ID、旧路径、重复修复和非定义同名标签。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformDefinitionIndexMutationTest,
    "GamePlatform.Data.Index.IncrementalAssetChanges",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformDefinitionIndexMutationTest::RunTest(const FString& Parameters)
{
    FIndexTestSource Source;
    const FAssetData First = MakeIndexTestAsset(TEXT("/Game/__DataIndex/A.A"), TEXT("test.first@1"));
    const FAssetData Duplicate = MakeIndexTestAsset(TEXT("/Game/__DataIndex/B.B"), TEXT("test.first@1"));
    Source.Assets = {First};
    FDefinitionSourceIndex Index;
    Index.Startup(Source.MakeAccess());
    FSoftObjectPath Path;
    TestTrue(TEXT("初始唯一"), Index.Resolve(IndexTestId(TEXT("test.first@1")), Path).IsSuccess());

    Index.OnAssetAddedOrUpdated(Duplicate);
    TestEqual(TEXT("新增重复立即拒绝"), Index.Resolve(IndexTestId(TEXT("test.first@1")), Path).Code, FName(TEXT("DuplicateLogicalId")));
    TestTrue(TEXT("重复不任意挑选路径"), Path.IsNull());
    Index.OnAssetRemoved(Duplicate);
    TestTrue(TEXT("删除重复立即恢复"), Index.Resolve(IndexTestId(TEXT("test.first@1")), Path).IsSuccess());

    const FAssetData Changed = MakeIndexTestAsset(TEXT("/Game/__DataIndex/A.A"), TEXT("test.second@1"));
    Index.OnAssetAddedOrUpdated(Changed);
    TestEqual(TEXT("身份改变撤销旧ID"), Index.Resolve(IndexTestId(TEXT("test.first@1")), Path).Code, FName(TEXT("MissingDefinition")));
    TestTrue(TEXT("新ID可用"), Index.Resolve(IndexTestId(TEXT("test.second@1")), Path).IsSuccess());
    const FAssetData Renamed = MakeIndexTestAsset(TEXT("/Game/__DataIndex/Renamed.Renamed"), TEXT("test.second@1"));
    Index.OnAssetRenamed(Renamed, Changed.GetSoftObjectPath().ToString());
    TestTrue(TEXT("重命名仍唯一"), Index.Resolve(IndexTestId(TEXT("test.second@1")), Path).IsSuccess());
    TestEqual(TEXT("返回新物理路径"), Path, Renamed.GetSoftObjectPath());
    TestEqual(TEXT("旧路径已撤销"), Index.GetDiagnostics().SourceCount, 1);

    Index.OnAssetAddedOrUpdated(Renamed);
    TestEqual(TEXT("重复通知不增加源"), Index.GetDiagnostics().SourceCount, 1);
    const FAssetData Invalid = MakeIndexTestAsset(Renamed.GetSoftObjectPath().ToString(), TEXT("bad identity"));
    Index.OnAssetAddedOrUpdated(Invalid);
    TestEqual(TEXT("非法新标签撤销旧源"), Index.Resolve(IndexTestId(TEXT("test.second@1")), Path).Code, FName(TEXT("MissingDefinition")));
    Index.OnAssetAddedOrUpdated(Renamed);
    const FAssetData Unrelated = MakeIndexTestAsset(Renamed.GetSoftObjectPath().ToString(), TEXT("test.second@1"),
        UDataAsset::StaticClass()->GetClassPathName());
    Index.OnAssetAddedOrUpdated(Unrelated);
    TestEqual(TEXT("非定义资产同名标签不入索引"), Index.Resolve(IndexTestId(TEXT("test.second@1")), Path).Code, FName(TEXT("MissingDefinition")));
    TestEqual(TEXT("所有增删改名均不重扫全库"), Source.EnumerationCalls, 1);

    // 显式失效只撤销可见快照，不在事件中扫描；后续普通查询只重建一次。
    Source.Assets = {Renamed};
    Index.Invalidate();
    TestEqual(TEXT("失效本身不扫描"), Source.EnumerationCalls, 1);
    TestTrue(TEXT("失效后首查重建"), Index.Resolve(IndexTestId(TEXT("test.second@1")), Path).IsSuccess());
    TestTrue(TEXT("重建后暖查"), Index.Resolve(IndexTestId(TEXT("test.second@1")), Path).IsSuccess());
    TestEqual(TEXT("两次查询只新增一次扫描"), Source.EnumerationCalls, 2);
    return true;
}

// 候选对象只替换自身注册路径；改ID、冲突、非法候选和不带候选查询不得相互污染。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformDefinitionIndexCandidateTest,
    "GamePlatform.Data.Index.EditorCandidateReplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformDefinitionIndexCandidateTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGamePlatformDefinitionBase> Candidate(
        NewObject<UGamePlatformDefinitionBase>(GetTransientPackage(), NAME_None, RF_Transient));
    FGamePlatformId::TryParse(TEXT("test.old@1"), Candidate->LogicalId);
    const FSoftObjectPath CandidatePath(Candidate.Get());
    FIndexTestSource Source;
    Source.Assets = {MakeIndexTestAsset(CandidatePath.ToString(), TEXT("test.old@1"))};
    FDefinitionSourceIndex Index;
    Index.Startup(Source.MakeAccess());
    FSoftObjectPath Path;
    TestTrue(TEXT("候选替换自身不制造重复"), Index.Resolve(IndexTestId(TEXT("test.old@1")), Path, Candidate.Get()).IsSuccess());

    FGamePlatformId::TryParse(TEXT("test.new@1"), Candidate->LogicalId);
    TestEqual(TEXT("候选改名撤销本次旧ID"), Index.Resolve(IndexTestId(TEXT("test.old@1")), Path, Candidate.Get()).Code, FName(TEXT("MissingDefinition")));
    TestTrue(TEXT("新候选身份可用"), Index.Resolve(IndexTestId(TEXT("test.new@1")), Path, Candidate.Get()).IsSuccess());
    TestEqual(TEXT("候选路径保持"), Path, CandidatePath);
    TestTrue(TEXT("无候选仍读取注册快照"), Index.Resolve(IndexTestId(TEXT("test.old@1")), Path).IsSuccess());
    TestEqual(TEXT("候选覆盖不写回全局"), Index.Resolve(IndexTestId(TEXT("test.new@1")), Path).Code, FName(TEXT("MissingDefinition")));

    const FAssetData Other = MakeIndexTestAsset(TEXT("/Game/__DataIndex/Other.Other"), TEXT("test.new@1"));
    Index.OnAssetAddedOrUpdated(Other);
    TestEqual(TEXT("候选与另一路径真实冲突"), Index.Resolve(IndexTestId(TEXT("test.new@1")), Path, Candidate.Get()).Code, FName(TEXT("DuplicateLogicalId")));
    Index.OnAssetRemoved(Other);
    Candidate->LogicalId.Name.Empty();
    TestEqual(TEXT("非法候选仍替换自己的旧注册路径"), Index.Resolve(IndexTestId(TEXT("test.old@1")), Path, Candidate.Get()).Code, FName(TEXT("MissingDefinition")));
    TestEqual(TEXT("候选变化不重新扫描"), Source.EnumerationCalls, 1);
    return true;
}

// 重启发现不能暴露旧快照；构建中取消/Shutdown或重入失效都不得发布过期源。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformDefinitionIndexLifecycleTest,
    "GamePlatform.Data.Index.DiscoveryCancellationAndShutdown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformDefinitionIndexLifecycleTest::RunTest(const FString& Parameters)
{
    FIndexTestSource Source;
    const FAssetData Asset = MakeIndexTestAsset(TEXT("/Game/__DataIndex/A.A"), TEXT("test.first@1"));
    Source.Assets = {Asset};
    FDefinitionSourceIndex Index;
    Index.Startup(Source.MakeAccess());
    FSoftObjectPath Path;
    TestTrue(TEXT("先建立可见源"), Index.Resolve(IndexTestId(TEXT("test.first@1")), Path).IsSuccess());
    Source.bDiscovering = true;
    TestEqual(TEXT("后续发现拒绝旧快照"), Index.Resolve(IndexTestId(TEXT("test.first@1")), Path).Code, FName(TEXT("AssetRegistryNotReady")));
    TestEqual(TEXT("等待发现不扫描"), Source.EnumerationCalls, 1);
    TestEqual(TEXT("失效后清空路径"), Index.GetDiagnostics().SourceCount, 0);
    Source.bDiscovering = false;
    TestTrue(TEXT("发现完成后重建"), Index.Resolve(IndexTestId(TEXT("test.first@1")), Path).IsSuccess());

    Index.Invalidate();
    Source.DuringEnumeration = [&Index]() { Index.Invalidate(); };
    TestEqual(TEXT("枚举中变化取消本次发布"), Index.Resolve(IndexTestId(TEXT("test.first@1")), Path).Code, FName(TEXT("AssetRegistryNotReady")));
    TestEqual(TEXT("取消扫描不增加发布次数"), Index.GetDiagnostics().RebuildCount, uint64(2));
    Source.DuringEnumeration = {};
    TestTrue(TEXT("后续稳定查询可以恢复"), Index.Resolve(IndexTestId(TEXT("test.first@1")), Path).IsSuccess());

    Index.Invalidate();
    Source.DuringEnumeration = [&Index]() { Index.Shutdown(); };
    TestEqual(TEXT("枚举中关闭不发布旧源"), Index.Resolve(IndexTestId(TEXT("test.first@1")), Path).Code, FName(TEXT("AssetRegistryNotReady")));
    TestTrue(TEXT("关闭取消的输出为空"), Path.IsNull());
    const auto Stopped = Index.GetDiagnostics();
    TestFalse(TEXT("关闭后无活动状态"), Stopped.bActive);
    TestFalse(TEXT("关闭后无已建快照"), Stopped.bBuilt);
    TestEqual(TEXT("关闭后清空值索引"), Stopped.SourceCount, 0);
    Index.OnAssetAddedOrUpdated(Asset);
    Index.OnAssetRemoved(Asset);
    Index.OnAssetRenamed(Asset, Asset.GetSoftObjectPath().ToString());
    Index.Invalidate();
    Index.Shutdown();
    TestEqual(TEXT("关闭后查询被拒绝"), Index.Resolve(IndexTestId(TEXT("test.first@1")), Path).Code, FName(TEXT("AssetRegistryNotReady")));
    TestEqual(TEXT("关闭后事件不能复活索引"), Index.GetDiagnostics().AssetEventCount, Stopped.AssetEventCount);
    TestEqual(TEXT("关闭后不能重新枚举"), Index.GetDiagnostics().FullScanCount, Stopped.FullScanCount);

    Source.DuringEnumeration = {};
    Index.Startup(Source.MakeAccess());
    TestTrue(TEXT("显式重新启动可重新冷建"), Index.Resolve(IndexTestId(TEXT("test.first@1")), Path).IsSuccess());
    TestEqual(TEXT("新活动期只扫描一次"), Index.GetDiagnostics().FullScanCount, uint64(1));
    return true;
}
#endif

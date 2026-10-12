# Task 2：GamePlatformData唯一源索引实施报告

日期：2026-10-12。任务详细需求来源：同目录 `task-2-brief.md`。

## 1. 状态与范围

**源码、六项新Automation回归及插件文档已完成写入；轻量源码/兼容检查通过。本子任务未执行UE构建或Automation，动态通过状态待主代理统一验证。**

项目：`agent:freebooz-4b15fc0c7cf14418:divinebeastsworkspace-888d2615`。实际工作区：`E:/poject/feebooz/DivineBeastsWorkspace`。开始时分支为main，HEAD为 `7d44194333cda24f8a14495695e1d478deef5a97`；Data插件开始时无既有差异，工作区其他PCG任务及共享文档的未提交改动均保留。没有提交、推送、创建第二工程、启动UE、停止编辑器或派生代理。

全部文件通过Cwebcodex读取及canonical `edit_project_files` 修改；已有文件使用 `read_files.read_revision` 作为编辑围栏。直接编辑工具未提供时按已查明的manifest使用 `call_runtime_tool`。

### 实际文件清单

以下路径相对 `Game/Plugins/GamePlatform/Foundation/GamePlatformData/`：

| 文件 | 变更与职责 |
|---|---|
| `Source/GamePlatformData/Private/Validation/GamePlatformDefinitionSourceIndex.h` | 新增；模块私有索引、源访问合同、计数及生命周期 |
| `Source/GamePlatformData/Private/Validation/GamePlatformDefinitionSourceIndex.cpp` | 新增；真实Registry适配、双向索引、增量变更、延迟冷建、候选解析、取消发布及解绑 |
| `Source/GamePlatformData/Private/Validation/GamePlatformDefinitionValidation.cpp` | 保持公开函数签名，转发到模块私有索引 |
| `Source/GamePlatformData/Private/GamePlatformDataModule.cpp` | 模块启动注册事件，关闭清理索引；不加载业务资产 |
| `Source/GamePlatformData/Private/Tests/GamePlatformDefinitionSourceIndexTests.cpp` | 新增四项生产索引行为测试 |
| `Source/GamePlatformData/Private/Tests/GamePlatformDefinitionSourceRegistryTests.cpp` | 新增两项实际Registry编辑器事件/委托解绑回归 |
| `README.md` | 当前索引入口、边界、源文件清单与新旧证据分期 |
| `Docs/Architecture.md` | 第7节完整索引行为与生命周期；更新相关旧限制 |
| `Docs/API.md` | 保持签名，说明唯一源查询行为及错误兼容 |
| `Docs/TestingAndEvidence.md` | 新增第9节，六个精确名称、参数、待执行入口和证据边界 |

另新增本报告：`Docs/Implementation/PluginArchitectureOptimization/task-2-report.md`。

目录规划和共享执行台账由主代理更新。需登记的新增源码只有上表四个新增文件；没有修改Public、Loading、Subsystems、DataEditor、Build.cs、uplugin或任何资产。

## 2. 生产设计与兼容边界

### 2.1 私有数据所有权

`FDefinitionSourceIndex` 同时保存：

- `LogicalId → TSet<FSoftObjectPath>`：保留全部物理源，以明确识别重复。
- `FSoftObjectPath → 原LogicalId`：按路径撤销旧源，避免身份更新后仍能通过旧ID解析。

索引只拥有值数据和引擎进程注册表的非拥有引用；不保存定义UObject、世界、PIE实例、请求或租约。不建立第二个资产管理器、加载器、Subsystem、Ticker或后台工作线程。

### 2.2 冷建、暖查询与事件

模块Startup只绑定委托。首次查询必须同时通过 `!IsLoadingAssets()` 与 `!IsGathering()`；此后一次 `GetAssetsByClass(UGamePlatformPrimaryDataAsset,...,true)` 取得完整源快照。Cooked运行在发现已完成时可直接冷建，不要求在索引创建之后再次收到FilesLoaded事件。

暖查询仅按规范LogicalId查集合。普通资产事件同步增量处理：

| 事件 | 行为 |
|---|---|
| AssetAdded / AssetUpdated / AssetUpdatedOnDisk | 先移除同路径旧身份，再根据类资格和合法LogicalId加入新身份 |
| AssetRemoved | 仅按路径撤销；不依赖事件中的LogicalId |
| AssetRenamed | 先撤销旧路径及新路径旧记录，再按新元数据加入 |
| ScanStarted / FilesLoaded / KnownGathersComplete | 立即清空旧快照；下次就绪查询才重新枚举 |
| 蓝图生成类标签变化 | 其他源的继承资格可能变化，明确失效后延迟重建 |

同一路径重复通知保持幂等。重复删除后恢复唯一，LogicalId变化后旧ID不再保留源，非法ID或非定义类更新会撤销该路径旧记录。

源枚举在局部构建快照；资产事件、失效或Shutdown推进代次。代次变化后拒绝发布整批结果。这里取消的是快照发布，不承诺中断已经执行中的同步Registry枚举；索引没有异步扫描任务残留。

### 2.3 类资格与UE5.8核对

实际锁定源码根为 `F:/UnrealEngine-5.8.0-release`，已读取其 `Engine/Build/Build.version`，版本为 **5.8.0**。

已加载类使用 `Asset.GetClass(EResolveClass::No)->IsChildOf(UGamePlatformPrimaryDataAsset::StaticClass())`。未加载类使用Registry的递归类过滤，对传入单条元数据调用 `RunAssetsThroughFilter`，不调用资产或类同步加载。仅具有 `GamePlatformLogicalId` 标签的普通资产不会混入。

只读核对的引擎位置：

| 引擎源码 | 已确认内容 |
|---|---|
| `Engine/Source/Runtime/AssetRegistry/Public/AssetRegistry/IAssetRegistry.h` | 八个所用委托、GetAssetsByClass、RunAssetsThroughFilter、AssetUpdateTags、AssetRenamed、WaitForCompletion |
| 同上，IsLoadingAssets/IsGathering注释 | IsLoadingAssets仅首次发现；IsGathering还覆盖后续挂载 |
| `Engine/Source/Runtime/AssetRegistry/Private/AssetRegistry.cpp` | CanBroadcastEvents要求GameThread且非PostLoad路由；后台事件延后；AssetCreated/Deleted/Renamed有GameThread约束 |
| `Engine/Source/Runtime/CoreUObject/Public/AssetRegistry/AssetData.h` | FAssetData构造、GetClass默认不加载、IsInstanceOf和类路径 |
| `Engine/Source/Runtime/CoreUObject/Public/Blueprint/BlueprintSupport.h` | FBlueprintTags::GeneratedClassPath |
| `Engine/Source/Runtime/Core/Public/Delegates/MulticastDelegateBase.h` | 真实委托IsBoundToObject核验入口 |
| `Engine/Source/Runtime/Core/Public/Templates/UnrealTemplate.h` | TGuardValue实际位置；本次静态复核修正了最初错误的GuardValue包含路径 |

### 2.4 公共行为与外部加载兼容

`ResolveUniqueGamePlatformDefinitionSource` 的Public头、签名和默认候选参数均未修改。

| 条件 | 保留行为 |
|---|---|
| 非法主资产类型或逻辑ID | InvalidDefinitionId |
| 发现未完成 | AssetRegistryNotReady；原服务继续延后重试并允许取消 |
| 无源 | MissingDefinition |
| 多物理源 | DuplicateLogicalId，不按遍历顺序任选 |
| EditorCandidate | 先剔除自身注册路径，再按其当前ID加入本次查询；不写回全局快照 |
| 失败 | 清空输出路径 |
| 唯一源与AssetManager映射不一致 | 原AddDemand仍返回PrimaryAssetMappingMismatch |

`UGamePlatformAssetManager::AddDemand` 的源校验调用、主资产映射核对、需求账本及后续加载逻辑原样保留。外部 `LoadPrimaryAssets / ChangeBundleStateForPrimaryAssets / UnloadPrimaryAssets` 基线机制没有更改。定义/普通资源租约、签发身份、多实例与世界清理均保持原实现。

所有索引入口仅GameThread。Shutdown先停止活动及取消发布，显式解除八个真实委托，再清空两个表和访问回调；若引擎Registry已销毁，不重新加载模块。静态析构在模块已关闭时不再访问引擎线程或注册表状态。

## 3. 新增Automation精确名称与覆盖

**下列六项是已写入源码、尚未由本子任务运行的测试。128/64是测试输入，不是实际通过数量。**

| 完整测试名称 | 主要断言 |
|---|---|
| `GamePlatform.Data.Index.ColdWarmRepeatedQueries` | 发现中不扫描；冷建一次；128次暖查/缺失/非法查询不新增枚举；同物理记录去重 |
| `GamePlatform.Data.Index.IncrementalAssetChanges` | 增删、重复修复、LogicalId变更、重命名、非法新标签、非定义标签；普通事件不全扫；失效后首查重建一次 |
| `GamePlatform.Data.Index.EditorCandidateReplacement` | 候选替换自身、改ID、与别源冲突、非法候选；共享索引未被候选写回 |
| `GamePlatform.Data.Index.DiscoveryCancellationAndShutdown` | 后续发现撤销旧快照；枚举中失效或关闭不能发布旧源；停止后事件不复活；显式重启 |
| `GamePlatform.Data.Editor.SourceIndexRegistryEvents` | 真实GUID内存定义的AssetCreated/Deleted/UpdateTags/Renamed，公开入口与本地索引一致；64次暖查；清理后重建及测试根无可见源 |
| `GamePlatform.Data.Editor.SourceIndexRegistryShutdown` | 核对八个真实委托IsBoundToObject，关闭后全部解绑；关闭后真实新增/更新/改名/删除不改变旧实例；重启和最终清理 |

独立行为测试只注入发现状态与源元数据，始终执行同一个生产索引算法。编辑器回归复用既有新建内存包、强引用和 `AssetCreated/AssetDeleted` 模式；增加真实 `AssetUpdateTags(..., FullUpdate)` 及 `AssetRenamed`。不存在保存、覆盖或伪造 `.uasset` 的操作。

非定义/未知类负例只把构造的元数据交给本地私有索引实例，使用真实Registry类资格适配，但不广播给其他插件。清理执行AssetDeleted、去除Public/Standalone、MarkAsGarbage及测试根可见性检查。引擎内部原始磁盘标签缓存的立即物理回收不是本测试承诺；Registry的EmptyPackage规则及重新冷建后不复活源是本回归的实际断言。

## 4. 已实际执行的轻量验证

| 命令/入口 | 实际结果与范围 |
|---|---|
| `git diff -- Game/Plugins/GamePlatform/Foundation/GamePlatformData`（编辑前） | 成功，无差异输出；确认Data范围没有待保护的既有改动 |
| `git diff --check -- Game/Plugins/GamePlatform/Foundation/GamePlatformData` | 退出0；无空白错误输出 |
| `git diff --exit-code HEAD -- <本报告附录A中的Public/Loading/Subsystems/DataEditor/Build.cs/uplugin精确路径>` | 退出0；公共与加载/租约/外部基线实现未变 |
| Cwebcodex `run_script(language="python", purpose="validation", assertion_name="Task2DataIndexSourceAndCompatibility")`，正文见附录A | 实际返回PASS；检查6个新/修改源码文件、直接包含缺失0、事件注册/解除8对、源全枚举调用点1、精确新增Automation身份6 |
| `git status --short -- Game/Plugins/GamePlatform/Foundation/GamePlatformData` | 6个已跟踪文件修改＋4个新增源码，均为本任务清单 |
| 引擎头文件/实现的定向 `rg -n` 与只读Python范围读取 | 核对真实5.8.0接口和广播线程，没有构建、启动、保存资产等副作用 |

首次按README历史路径 `D:/UnrealEngine-5.8.0-release` 读取引擎头失败，`rg`退出2，明确为路径不存在。之后从实际引擎注册信息及主代理确认改用已存在的F盘锁定根，不更换引擎版本。检查过程中发现本增量最初写入的 `Misc/GuardValue.h` 不存在，已改为实际 `Templates/UnrealTemplate.h`；最终包含检查无缺失。

只读源码、结构检查不能证明C++成功编译、测试运行或Cooked资产正确；未把上述PASS记为UE通过。

## 5. 主代理待执行验证与所需参数

新六项不需要外部Definition参数；完整Data前缀回归保留以下已存在测试身份及夹具要求：

- `GamePlatform.Data.Definition.IdentityAndVersion`
- `GamePlatform.Data.Editor.SourceDuplicate`
- `GamePlatform.Data.Editor.DependencyGraph`
- `GamePlatform.Data.Editor.DependencyDepthLimit`
- `GamePlatform.Data.Editor.RegisteredValidatorDispatch`
- `GamePlatform.Data.Runtime.ResourceLeases`
- `GamePlatform.Data.Runtime.RealAssetLeases`
- `GamePlatform.Data.Runtime.RecursiveFailureRollback`
- `GamePlatform.Data.Runtime.DeferredRequeueZeroDelta`
- `GamePlatform.Data.Runtime.NestedTickerNotification`

所有原有测试文件均未更改。RealAssetLeases仍需 `-GamePlatformDataTestDefinition=GamePlatformDefinition:foundation.probe@1` 或明确存在的独占开发定义。原测试包含两隔离GameInstance/世界生命周期；本次检索未发现Data目录内另一个独立命名的PIE测试，不把多实例模拟等同真实PIE会话验证。

### 待执行命令模板（没有由本子任务运行）

按主代理统一调度执行；不得因这些模板并行启动构建或关闭其他编辑器。

```powershell
& 'F:/UnrealEngine-5.8.0-release/Engine/Build/BatchFiles/Build.bat' DivineBeastsArenaEditor Win64 Development '-Project=E:/poject/feebooz/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject' -Module=GamePlatformData -WaitMutex
& 'F:/UnrealEngine-5.8.0-release/Engine/Build/BatchFiles/Build.bat' DivineBeastsArenaClient Win64 Development '-Project=E:/poject/feebooz/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject' -Module=GamePlatformData -WaitMutex
& 'F:/UnrealEngine-5.8.0-release/Engine/Build/BatchFiles/Build.bat' DivineBeastsArenaServer Win64 Development '-Project=E:/poject/feebooz/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject' -Module=GamePlatformData -WaitMutex
& 'F:/UnrealEngine-5.8.0-release/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/poject/feebooz/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject' -unattended -nop4 -NullRHI -nosplash -stdout -FullStdOutLogOutput '-GamePlatformDataTestDefinition=GamePlatformDefinition:foundation.probe@1' '-ExecCmds=Automation RunTests GamePlatform.Data' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=E:/poject/feebooz/DivineBeastsWorkspace/Game/Saved/Automation/PluginArchitectureOptimization/DataIndex'
```

主代理需要在本次导出中逐项核对6个新增＋10个保留身份被发现、完成且Success，记录真实退出码及日志。现有 `Build/Validation/TestFoundationUnreal.ps1` 可选Data前缀，但其历史必需名单不含新六项；不能只依赖旧名单或数量。该脚本要求PowerShell7，不能用默认5.1绕过版本要求。

## 6. 疑虑、限制与交付边界

1. **UE编译/Automation未由本子任务执行。** 包含可解析只是一项轻量检查，模板/链接/运行时问题仍须统一构建和实际测试结果排除。
2. **真实Cooked发现及未加载蓝图派生定义未动态验收。** 代码使用已核对的注册表递归类机制，不同步加载类；本次未知类负例只验证拒绝错误类，不能替代真实派生蓝图及Cooked扫描场景。
3. **真实长时间磁盘发现、OnAssetUpdatedOnDisk物理磁盘变化和PIE会话尚未运行。** 生产绑定对应事件；独立可控源覆盖未就绪/重入取消，内存Registry回归覆盖实际新增/标签更新/改名/删除。
4. **性能主张限于减少重复全枚举的实现及可执行计数断言。** 冷建和发现/类变化后的重建仍有元数据枚举成本；没有实际CPU、内存、耗时或性能百分比。
5. **外部兼容基线原样保留。** 仍需完整RealAssetLeases/RecursiveFailureRollback等回归检验新源索引在原加载链中的行为；本次不改变外部加载保守基线策略。
6. **非本范围文档问题已告知主代理。** 初次读取的 `Docs/Implementation/GamePlatformAuditRemediation/整合与交付说明_2026-10-09.md` 尾部含既有合并标记，主代理表示由Task5处理；本子任务未编辑该文件。
7. 中文职责、GameThread约束、源/世界所有权、失败与取消发布、八委托释放、测试夹具/参数和验证边界已在涉及文件补齐。未做全工程历史中文注释合规认证。
8. 代码与轻量验证不代表Cook/Stage、图形、联机、设备性能、全量内容或发布验收；本任务没有改变47个代码插件、17个内容包或三层边界。

## 附录A：实际执行的只读源码/兼容核对脚本

执行入口为上文Cwebcodex `run_script`（Python），实际结果为PASS；脚本没有源码写入、UE启动或资产保存操作。

```python
from pathlib import Path
import json, re, subprocess
plugin = Path("Game/Plugins/GamePlatform/Foundation/GamePlatformData")
module = plugin / "Source/GamePlatformData"
new_files = [
    module / "Private/Validation/GamePlatformDefinitionSourceIndex.h",
    module / "Private/Validation/GamePlatformDefinitionSourceIndex.cpp",
    module / "Private/Tests/GamePlatformDefinitionSourceIndexTests.cpp",
    module / "Private/Tests/GamePlatformDefinitionSourceRegistryTests.cpp",
]
changed_source = new_files + [
    module / "Private/GamePlatformDataModule.cpp",
    module / "Private/Validation/GamePlatformDefinitionValidation.cpp",
]
engine = Path("F:/UnrealEngine-5.8.0-release")
roots = [module / "Public", module / "Private",
    plugin.parent / "GamePlatformCore/Source/GamePlatformCore/Public",
    engine / "Engine/Source/Runtime/Core/Public",
    engine / "Engine/Source/Runtime/CoreUObject/Public",
    engine / "Engine/Source/Runtime/Engine/Public",
    engine / "Engine/Source/Runtime/Engine/Classes",
    engine / "Engine/Source/Runtime/AssetRegistry/Public"]
missing = []
for path in changed_source:
    text = path.read_text(encoding="utf-8-sig")
    for include in re.findall(r'^#include "([^"]+)"', text, re.M):
        if not any((root / include).is_file() for root in roots):
            missing.append({"file": str(path), "include": include})
assert not missing, missing
check = subprocess.run(["git", "diff", "--check", "--", str(plugin)], capture_output=True, text=True)
assert check.returncode == 0, check.stdout + check.stderr
unchanged = [module / "Public", module / "Private/Loading", module / "Private/Subsystems",
    plugin / "Source/GamePlatformDataEditor", module / "GamePlatformData.Build.cs",
    plugin / "GamePlatformData.uplugin"]
compat = subprocess.run(["git", "diff", "--exit-code", "HEAD", "--"] + [str(p) for p in unchanged],
    capture_output=True, text=True)
assert compat.returncode == 0, "Public/loading/lease/editor/binding baseline unexpectedly changed"
names = []
for path in new_files[2:]:
    names += re.findall(r'IMPLEMENT_SIMPLE_AUTOMATION_TEST\(\s*\w+,\s*"([^"]+)"',
        path.read_text(encoding="utf-8-sig"), re.S)
assert len(names) == len(set(names)) == 6, names
source = new_files[1].read_text(encoding="utf-8-sig")
assert source.count(".GetAssetsByClass(") == 1
assert ".OnAssetAdded().AddRaw" in source and ".OnAssetAdded().Remove" not in source
for name in ["AssetAdded", "AssetRemoved", "AssetUpdated", "AssetUpdatedOnDisk", "AssetRenamed",
             "FilesLoaded", "ScanStarted", "KnownGathersComplete"]:
    assert f"On{name}().AddRaw" in source and f"On{name}().Remove" in source, name
version = json.loads((engine / "Engine/Build/Build.version").read_text(encoding="utf-8-sig"))
print(json.dumps({
    "check": "Task2 source and compatibility audit",
    "status": "PASS",
    "engine": f"{version['MajorVersion']}.{version['MinorVersion']}.{version['PatchVersion']}",
    "source_files_checked": len(changed_source),
    "missing_direct_includes": missing,
    "git_diff_check_exit": check.returncode,
    "unchanged_public_loading_lease_editor_rules_exit": compat.returncode,
    "registry_full_enumeration_callsites": 1,
    "paired_registry_event_names": 8,
    "new_automation_tests": names,
    "not_executed": ["UE compilation", "UE Automation", "Cook/Stage", "PIE", "graphics/network"]
}, ensure_ascii=False, indent=2))
```

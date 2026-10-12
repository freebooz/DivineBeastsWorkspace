# Task 1 独立审查记录

## 结论与证据边界

2026-10-12 对服务器表现资源排除方案进行了独立只读审查。审查基线为工作区 HEAD `f2cf1b6fcdcaa68ecb9c96e7cabef62fa4d0d740`；工作区当时已有其他任务改动。发现 **两类需要在集成前修复的问题**：

1. 开发技能权威目录的保护能被大小写及祖先目录绕过。
2. 受管 INI 段外覆盖检查漏掉真实 UE/UBT 支持的清除写法。

两项均已把生产 Python API 的接受行为复现出来；第二项的真实消费者行为由锁定 UE 5.8 源码确认。未修改生产代码、配置、资产、测试、共享目录规划或执行台账；本次唯一仓库写入是此审查记录。未启动 UE、UBT、Cook、UnrealPak 或其他编辑器进程，未提交或推送。

另记录一个容器数据尺寸的具体疑虑，供实施者与根代理裁定验证边界。没有将人工 CSV 或 mock 容器结果写成真实 Cook/Stage 证据。

## 审查对象

实际阅读了以下内容，而不是仅按报告判断：

- `Docs/Implementation/PluginArchitectureOptimization/task-1-brief.md`、`task-1-report.md`。
- `Build/Game/ServerPresentationPolicy.py`：加载、权威目录保护、受管段生成/漂移检测、路径和外置对象映射、Pak/IoStore 解析、Stage 枚举及命令入口。
- `Build/Game/ServerPresentationExclusions.json` 及 `ServerPresentationExclusions.schema.json`：三配置映射、FoundationStandalone 服务器别名、Cook 与 Stage-only 边界、LegacyAbsent。
- `Build/Game/CookFoundation.ps1`：实际参数、预检时机、AdditionalCookerOptions、Stage 收据配置边界、逐容器审计调用与退出码。
- DedicatedServer、VillageServer、VillageDevelopmentServer 的配置与共享 FoundationStandalone 配置；对照 `a5fca499` 至 `f2cf1b6f` 返回的配置差异及旧 DedicatedServer Pak 文件。
- `Tests/Foundation/Scripts/TestServerPresentationPolicy.py`、`TestFoundationPackagingConfig.ps1`，以及前者当前未提交差异。
- Common/十二英雄包的实际资产文件名清单；没有读取二进制资产的运行时依赖图。
- 锁定引擎 `F:/UnrealEngine-5.8.0-release` 中的 `ConfigCacheIni.cpp`、UBT `Configuration/Ini/ConfigFile.cs`、`IoStoreUtilities.cpp` 和 Pak 日志格式。

本次没有为了重复计数再运行既有 21 项测试，也没有重复根代理的 Editor 构建。

## B1：开发技能权威目录保护不完整（集成前修复）

**位置：** `Build/Game/ServerPresentationPolicy.py:347–353`，尤其 351–353。既有测试 `TestServerPresentationPolicy.py:91–95` 没有覆盖该分支的大小写及祖先路径。

### 触发条件与实际结果

向 JSON 的 CookDirectories 加入以下任一规则，其他合法字段保持与当前规则一致：

| 变体 | PackagePath | SourceDirectory |
|---|---|---|
| 大小写 | `/Game/Development/DivineBeasts/abilities` | `Game/Content/Development/DivineBeasts/abilities` |
| 祖先目录 | `/Game/Development/DivineBeasts` | `Game/Content/Development/DivineBeasts` |

在当前 Windows 工作区上，大小写变体的目录 `is_dir()` 为 True。两个变体都通过实际 `load_policy()`，随后：

```python
policy.is_forbidden(
    "/Game/Development/DivineBeasts/Abilities/DA_AbilitySet_Dev.uasset",
    "VillageDevelopmentServer")
# True
```

这是合成资产名用于观察路径许可的断言；未宣称这个名称的资产本身存在。开发技能目录及其祖先目录确实存在，加载器使用的目录存在性检查没有被 mock。

### 根因与影响

一般 AUTHORITY_ROOTS 的检查已经使用 casefold 和双向目录包含，但 Abilities 是后面的特例：

```python
if _under(package, "/Game/Development/DivineBeasts/Abilities") and not _under(
        package, "/Game/Development/DivineBeasts/Abilities/UI"):
```

该特例既没有 casefold，也不拒绝其祖先目录。祖先 `/Game/Development/DivineBeasts` 不在前面的危险根枚举中，因此无需大小写变化即可绕过。

错误规则可以进入生成的 NeverCook 与 Pak 并集，误排应保留的开发技能权威资产，抵消开发配置对 Abilities 的 AlwaysCook/精确解除排除意图。**当前 JSON 本身仍只有允许的 UI 子目录；本发现针对生成器应拒绝危险规则的契约，不是声称当前配置已把全部技能排除。**

### 最小修复

先把待比较包路径与受保护 Abilities/UI 常量统一 casefold。除明确位于 UI 子树内的规则外，拒绝与 Abilities 根有任意包含关系的规则，包括相等、祖先、子孙；不扩大 UI 例外到 Abilities 的其他同级目录。

补 RED→GREEN 回归：标准大小写 Abilities 根、大小写变体、DivineBeasts 祖先必须拒绝；当前精确 UI 规则仍应接受；权威技能路径仍应允许。

## B2：受管段检查与真实 UE/UBT 指令语义不一致（集成前修复）

**位置：** `Build/Game/ServerPresentationPolicy.py:227–236`、`249–251`。当前测试 `TestServerPresentationPolicy.py:136–151` 只覆盖部分等号指令和键名前后空白。

### B2a：Cooker 的 ~! 清除指令通过预检

在 VillageServer 的受管段之后、同一 Packaging 节下添加：

```ini
~!DirectoriesToNeverCook=ClearArray
```

实际 `render_config("VillageServer", text)` 接受并原样保留该行。由于生成段本身没有变化，`check_configs()` 的字节比较也不会因此发现漂移。

UE 5.8 原生消费者的行为有明确源码依据：

- `Engine/Source/Runtime/Core/Private/Misc/ConfigCacheIni.cpp:2069–2074` 先跳过开头的 `~`。
- 同文件 2076–2096 随后处理 `!` 等符号命令。
- Python 227 的正则只接受直接紧邻键名的 `!+.-`，漏掉 `~` 包装。

因此原生 Cooker 会执行清空数组，而策略预检仍通过。后置 Stage 排除可能仍剥离表现文件，但这不能替代“漂移时不开始 Cook”的前置契约。

### B2b：Pak 消费者支持运算符后空白和无等号清除

在 DedicatedServer 的受管 Pak 段之后、同一节下添加任一项：

```ini
! Files=ClearArray
!Files
```

实际 `render_pak(text)` 对两项均接受并原样保留。

UE 5.8 UBT 消费者：

- `Engine/Source/Programs/UnrealBuildTool/Configuration/Ini/ConfigFile.cs:398–405` 在移除 `+/-/!` 后跳过空格和 Tab。
- 同文件 408–412 允许 RemoveKey 不带等号和值。
- Python 249–250 要求符号直接连着键名且后面存在 `=`，两种真实清除写法都漏检。

清空 Files 会移除整个受管 Pak 文件匹配数组；后置审计可能随后失败，但 Stage 配置已不再代表真源，预检不能声称一致。

**语义区别：** 本次还观察了 `! DirectoriesToNeverCook=...` 的 Python 接受行为。原生 C++ 在移除运算符后没有同样的空白处理，因此没有把这一项当作原生 Cooker 清除复现；运算符后空白的真实影响证据针对 UBT/Pak。避免用 UBT 解析成功代替原生 Cooker 的等价性证明。

### 最小修复

对受管键的段外指令先做与消费端一致的词法识别，再进行禁止清除/覆盖和路径检查。至少覆盖原生 `~` 包装、UBT 运算符后空白、UBT 无等号 RemoveKey。不要仅在当前正则补一个孤立示例而继续默认未知前缀无害。

UE 原生识别集合还出现 `@/*/^`；本审查按根代理要求收敛，未把其每种合并效果推导或运行成独立发现。修复时应核对这些前缀对受管键的语义，保守拒绝可改变受管数组语义的段外命令，同时保留不相关节。

补 RED→GREEN 回归的最小集合：

- Packaging 同节 `~!DirectoriesToNeverCook=ClearArray` 必须被拒绝。
- Pak 同节 `! Files=ClearArray`、`!\tFiles=ClearArray`、`!Files` 必须被拒绝。
- 同节换行重复节名仍拒绝，不相关节相同键仍保持原文。
- 继续保留开发 Abilities 的合法精确解除排除，不清空或重写其他数组。

## C1：UCAS 数据尺寸与清单范围尚未核对（具体疑虑，交根代理裁定）

**位置：** `Build/Game/ServerPresentationPolicy.py:396–417`、`504–515`。

当前对 UCAS 只检查存在且字节数大于零；CSV 读取丢弃 OffsetOnDisk、CompressedSize、PartitionIndex、OffsetInPartition 等列。锁定引擎的 `IoStoreUtilities.cpp:6214–6242` 读取并枚举 TOC，`6253–6275` 输出元数据清单，不读取 UCAS 包负载。因此有效 TOC 配合被截短但仍非空的 UCAS 可以产生完整、无 Error、退出 0 的清单。

本次临时夹具调用了实际 `audit_stage()`，仅 mock `_inspect_container`，没有启动引擎：唯一非空 Server exe、非空 UTOC、**1 字节 UCAS**，完整 19 列 CSV 声称本次要求地图有 **80 字节非空包数据**，实际返回 `Status=Passed`。这证明策略函数没有范围检查；它不是一次真实损坏容器实验。

报告已经说明没有全量哈希/解压验证。本疑虑更窄：可考虑利用真实 TOC/CSV 的分区与压缩范围核查底层数据文件大小，拒绝缺分区或明显截短，无需全量解压。若本轮明确只保证索引可列出，应把“损坏容器拒绝”的验收措辞限定到已实现的索引、日志与清单校验，避免将非空 UCAS 等同于完整可读数据。按根代理要求未继续扩大实验或修改实现。

## 实际诊断命令与结果

诊断统一使用：

```text
F:/UnrealEngine-5.8.0-release/Engine/Binaries/ThirdParty/Python3/Win64/python.exe -B -c <下述只读诊断脚本>
```

通过 `importlib.util.spec_from_file_location` 加载当前生产脚本；`-B` 禁止写入 bytecode。JSON 变体仅用 `unittest.mock.patch.object(api.json, "loads", return_value=raw)` 注入内存，不写真实规则文件。目录存在性仍查询真实工作区。

| 实际诊断标识 | 本次新增观察 | 结果 |
|---|---|---|
| Task1ReviewBoundaryCases | 大小写 Abilities 目录存在性及 load_policy/is_forbidden；记录运算符空白接受行为 | 命令成功；`authority_case_variant_dir_exists=True`；`authority_case_variant_rule_accepted=True` |
| Task1ReviewBoundaryCases2 | `~!` 原样接受；祖先 DivineBeasts 规则接受 | 命令成功；`tilde_clear_accepted=True`；`authority_ancestor_rule_accepted=True` |
| Task1ReviewPakOverridesAndPayloadBounds | Pak 两种清除指令；实际 audit_stage 的 UCAS 尺寸夹具 | 命令成功；两项 `pak_unmanaged_accepted=True`；`ucas_1_byte_vs_csv_80_bytes_status=Passed`；`no_engine_process_launched=True` |

成功是诊断命令运行成功，输出的 True/Passed 正是被审查的错误接受行为，不代表产品通过。最后一项只在系统 TemporaryDirectory 创建并清理合成证据文件。

最小 API 复现骨架如下；两个 JSON 变体和三种清除文本见前文表格：

```python
raw = copy.deepcopy(json.loads(policy_path.read_text(encoding="utf-8-sig")))
raw["CookDirectories"].append({
    "PackagePath": package_path,
    "SourceDirectory": source_directory,
    "Profiles": "AllServers",
    "Availability": "Present",
    "Reason": "review fixture",
})
with patch.object(api.json, "loads", return_value=raw):
    accepted = api.load_policy(policy_path, workspace)

policy.render_config("VillageServer", existing_village_ini + "\n~!DirectoriesToNeverCook=ClearArray\n")
policy.render_pak(existing_pak_ini + "\n! Files=ClearArray\n")
policy.render_pak(existing_pak_ini + "\n!Files\n")
```

阅读引擎及 Git 的命令属于只读 `rg`、`git show`、`git diff` 和定界行读取。曾对旧 UBT `System/ConfigFile.cs` 位置做一次不存在路径探测，返回 2；随后用 `rg --files` 找到实际 `Configuration/Ini/ConfigFile.cs`。该失败没有被当成任何验证通过证据。

## 已确认的设计与尚未验证的边界

- 三个服务器 profile 与 FoundationStandalone 的有限映射清楚；FoundationStandalone 的共享 INI 没有被加入服务器表现排除。CookFoundation 的实际 Server 分支追加独立 Cooker token，Client 分支不取得服务器策略。测试从实际 AST 提取对应装配语句，检查的是这些语句而非另外手写的一组参数。
- CookFoundation 把 CustomConfig 放入 AdditionalCookerOptions；开发 Stage 的两项 AllowedConfigFiles 在 UAT 参数中独立追加。当前代码、既有参数测试与报告在这部分相符；未实际运行 UAT 验证命令行穿透。
- 最终 Stage 规则使用三配置并集，与共享 DedicatedServer 收据一致。开发技能 UI 因此对三种 Stage 均禁入；权威技能父目录保持许可是必须保护的约束。
- 边界匹配使用目录段；虚拟包路径、实体插件/项目路径、大小写、分隔符和同目录外置 Actor/Object 均有明确处理。没有从本次阅读中发现现有规则把 Snowfall 当成 Snow 的问题。
- 当前 JSON 的 Common/十二英雄 Stage-only 根延续原有范围，Heroes 通配已经变成明确插件列表。清单中实际有 Common 动画资产；本次只读源码/文件名审查不能证明所有骨骼动画均无服务器引用。没有新的二进制依赖图证据，不能用合成的 DBAGameplay/Animations 路径测试替代真实权威动画保留验证。
- 每个发现的 pak/utoc 都有新的检查目录，未直接使用历史 CSV；空 pak/utoc、缺失/空主 UCAS、空/仅表头 CSV、Pak 缺失总数/字节摘要等已有显式拒绝。未运行真实容器，不对实际 Pak/UTOC 兼容性作通过承诺。
- 原 21 项测试中的最后一项在当前工作区已改名为 `test_empty_stage_is_rejected_before_container_tool` 并收紧为空目录断言；这符合实际覆盖内容。当前测试文件仍没有完整的多容器 audit_stage 集成路径回归；建议修复阶段针对失败后不能沿用前一容器证据补测试，不以重复 21 项数量代替该行为。
- 真正 Cook/Stage、所选地图及权威资产的完整性、运行/联机仍由根代理统一验证。本审查没有干预其构建检出或其他 Editor。

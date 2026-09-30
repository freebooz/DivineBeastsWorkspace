# TestingAndEvidence（测试与证据）

版本：0.2.0｜2026-09-30

本文件严格区分静态门禁、真实 C++ 编译、Client Target、UE Automation、Cook、Review Map 与性能实测。

## 1. 当前自动化源码

C++ Automation 源码覆盖 Handle、参数 Schema、Generic Niagara 行为分类、Composite、Presentation Provider 与预算策略。`GamePlatformVFXScalabilityTests.cpp` 已增加 Soft Budget / Hard Safety Cap（软预算／绝对安全上限）规则；Composite 测试要求父实例总生命周期必须大于 0，避免无 Niagara 完成事件的父记录永久残留。

旧 Catalog/Resolver 测试只证明兼容层确定性行为，不代表标准 Gameplay 仍应使用第二套 VFX 语义目录。

## 2. 本轮静态验证

`Tests/Scripts/ValidateGamePlatformVFX.ps1` 已真实执行并通过，当前门禁覆盖：

- ClientOnly / Editor 端侧与 TargetAllowList；
- DBAClient 显式启用 GamePlatformVFX；
- Dedicated Server Target 不启用 DBAClient / GamePlatformVFX；
- VFX Definition 继承 `UGamePlatformDefinitionBase`；
- `VFXRuntime` Asset Bundle；
- `IGamePlatformDataService::AcquireDefinition`；
- `OnSystemFinished` 生命周期回收；
- Corrected 预测语义；
- Game/PIE/GamePreview World 限制；
- 私有 Definition Preloader 已退休；
- Editor Validation 遍历默认／平台／质量 Niagara 变体。

最新结果：`passed=true`，`binaryAssetsObserved=0`。

VFX Lib 第一批迁移后再次执行 `ValidateGamePlatformVFX.ps1`：**通过**。新增门禁覆盖通用 Shader 文件存在、`/Plugin/GamePlatformVFX` Shader 虚拟路径注册、14 个稳定 `User.*` 参数名、6 张 SourceArt 源素材清单、平台 Shader 禁止 Frost/Petal/DBA 等项目语义，以及 SourceArt 不得放入运行时 Content。

`git diff --check` 已真实执行通过；仅曾出现工作区 LF/CRLF 转换提示，不属于差异格式错误。

## 3. UE5.8 定向模块编译

已使用真实 UE5.8 工具链执行：

```text
DivineBeastsArenaEditor Win64 Development
-Module=GamePlatformVFXClient
-Module=GamePlatformVFXEditor
-NoUBA
```

首轮真实编译发现并修复两项迁移问题：

1. 旧兼容 `UGamePlatformVFXCatalog` 残留未声明 `GetPrimaryAssetId()` 实现；已删除自建 StableId PrimaryAsset 身份逻辑，回退到 `UPrimaryDataAsset` 默认身份。
2. `constexpr FName VFXRuntimeBundle` 不满足 UE5.8 常量表达式要求；已改为运行期只读 `const FName`。

修复后复跑结果：**Succeeded**。`GamePlatformVFXClient` 与 `GamePlatformVFXEditor` 均完成编译和 DLL 链接。

VFX Lib 第一批迁移后再次使用 UE5.8 定向编译，UHT 重新运行并编译 `GamePlatformVFXCommonParameters.cpp`、`GamePlatformVFXCommonParametersTests.cpp`、`GamePlatformVFXClientModule.cpp`，随后重新链接 `UnrealEditor-GamePlatformVFXClient.dll` 与 `UnrealEditor-GamePlatformVFXEditor.dll`，结果：**Succeeded**。这证明新增 RenderCore/Shader 映射和参数契约的 C++ 编译合同成立；`.ush` 只有在真实 Niagara Module/Custom HLSL 引用后才能形成 Shader 编译证据。

## 4. Client Target

已启动 `DivineBeastsArenaClient Win64 Development -Module=GamePlatformVFXClient`：

- Client Target 解析成功；
- `DBAClient` 组合关系生效；
- UHT 实际运行成功并生成 Client 目标代码；
- 随后进入 5 个 `GamePlatformVFXClient` C++ 编译动作。

本机该轮编译器／UBA 执行长时间无终态输出，为避免残留构建任务已主动停止。因此当前状态只能记录为：**Client Target UHT 通过，C++ 最终编译终态未验证**，不能记为 Client Build 通过或失败。

本次 VFX Lib 迁移没有重新执行完整 Client Target；上一轮 Client Target 状态保持不变。

## 5. UE Automation

历史首轮 `UnrealEditor-Cmd` 启动曾在测试队列开始前受 VisionOS SDK 校验阻断。性能整改后重新执行 `Automation RunTests GamePlatform.VFX`，引擎已越过该阶段，但在加载项目插件时因当前工程无关模块 `DivineBeastsApplicationFlowClient（神兽联盟应用流程客户端模块）` 无法加载而退出；日志中没有发现 VFX 测试用例开始执行。

因此当前状态仍为：**Automation 工程启动环境阻塞／未执行 VFX 测试用例**，不是 GamePlatformVFX 自动化断言失败。

新增/扩展的 `GamePlatform.VFX.Scalability.*`、`GamePlatform.VFX.Lifecycle.InstanceRegistry` 与 `GamePlatform.VFX.Parameters.CommonNames` 测试源码均已随 UE5.8 定向模块编译成功；测试用例本身尚未取得运行时通过证据。当前仓库也仍没有真实 Niagara `.uasset`。

## 6. 尚未取得的证据

```text
Server Target真实构建终态：未执行
Client/Server Cook：未执行
L_VFXReview真实评审地图：仓库无合法.umap，未执行
真实Niagara System / EffectType / Definition资产：当前0个；第一批SourceArt=6个，均位于非Cook目录
Multi-PIE / Travel：未执行
1v1 / 5v5 / OpenWorld / Village性能：未执行
Android设备Niagara Insights：未执行
```

Server 隔离当前只有源码和描述文件门禁，不得替代真实 Server Build/Cook/Stage 审计。

## 7. 结论

当前可以确认：**核心架构整改已落地，VFX Lib 第一批去主题化迁移已落地，静态门禁通过，UE5.8 Editor 定向模块编译通过。**

在 Client Target 终态、Automation、Cook、真实资产 Review Map 和性能工件完成前，不得描述为 Production Ready（生产就绪）。

## 8. 2026-09-30 性能整改

针对高频战斗热路径完成以下源码整改：

- per-instance `AcquireDefinition` 改为 World 共享 Definition Cache，同一 Definition 的并发 Play / Preload 复用一个 Lease；
- 缓存增加 `MaxCachedDefinitions` 容量边界和空闲 LRU 淘汰；
- Instance Registry 增加 Component→Handle 反向索引；
- Dedupe 增加 Handle→Key 反向索引，删除每次 Play 的全表 Prune；
- Composite 延迟 Timer 绑定 Parent Handle，可随取消/纠正/World退出清除；
- Critical 不再全局禁用 Niagara Pool；
- Ambient / Status / Combat 改为分层累计软预算；
- Editor Validator 增加 Definition EffectType 与所有 Niagara System 变体实际 EffectType 一致性校验；
- Definition 结构校验只在共享缓存首次加载时执行一次；
- Spawn 参数覆盖不再先复制合并六类 Parameter TMap；
- 增加 `stat GamePlatformVFX` 与 CPU Trace Scope 性能观测。

使用 UE5.8 执行 `DivineBeastsArenaEditor Win64 Development -Module=GamePlatformVFXClient -Module=GamePlatformVFXEditor`，17个Action全部编译并链接成功，结果 **Succeeded**。

新增/扩展自动化源码覆盖分层预算、Critical池化资格、Component反向索引。真实 UE Automation 执行结果仍需单独取得。

本次整改解决的是框架CPU/内存热路径，不等于真实Niagara内容GPU达标。仓库仍缺少真实 Niagara System / EffectType / Definition 和 Review Map，因此 1v1 / 5v5 / OpenWorld / Village / Android 性能验收继续保持“未执行”。
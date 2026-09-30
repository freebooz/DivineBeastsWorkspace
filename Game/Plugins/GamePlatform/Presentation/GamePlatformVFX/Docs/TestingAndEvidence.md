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

## 4. Client Target

已启动 `DivineBeastsArenaClient Win64 Development -Module=GamePlatformVFXClient`：

- Client Target 解析成功；
- `DBAClient` 组合关系生效；
- UHT 实际运行成功并生成 Client 目标代码；
- 随后进入 5 个 `GamePlatformVFXClient` C++ 编译动作。

本机该轮编译器／UBA 执行长时间无终态输出，为避免残留构建任务已主动停止。因此当前状态只能记录为：**Client Target UHT 通过，C++ 最终编译终态未验证**，不能记为 Client Build 通过或失败。

## 5. UE Automation

已真实启动 `UnrealEditor-Cmd` 运行 `GamePlatform.VFX.*`，但测试队列尚未开始前，引擎全平台 SDK 校验因本机缺失 VisionOS SDK 退出。日志同时确认 Win64、Android、Linux、LinuxArm64 SDK 有效。

因此当前状态为：**Automation 环境阻塞／未执行测试用例**，不是 VFX Automation 测试失败。

## 6. 尚未取得的证据

```text
Server Target真实构建终态：未执行
Client/Server Cook：未执行
L_VFXReview真实评审地图：仓库无合法.umap，未执行
真实Niagara System / EffectType / Definition资产：当前0个
Multi-PIE / Travel：未执行
1v1 / 5v5 / OpenWorld / Village性能：未执行
Android设备Niagara Insights：未执行
```

Server 隔离当前只有源码和描述文件门禁，不得替代真实 Server Build/Cook/Stage 审计。

## 7. 结论

当前可以确认：**核心架构整改已落地，静态门禁通过，UE5.8 Editor 定向模块编译通过。**

在 Client Target 终态、Automation、Cook、真实资产 Review Map 和性能工件完成前，不得描述为 Production Ready（生产就绪）。
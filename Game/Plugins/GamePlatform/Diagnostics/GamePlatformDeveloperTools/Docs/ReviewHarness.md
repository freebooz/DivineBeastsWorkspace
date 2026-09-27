# Plugin Review Harness（插件独立演示与人工审核宿主）设计

> 状态：P0-8 设计；复用已有 GamePlatformDeveloperTools Review 类型，不新建 Review 插件。

## 1. 已有能力

当前已有：
- `UGamePlatformReviewCase`：CaseId、Category、前置、步骤、期望结果、RequiredEvidence。
- `UGamePlatformReviewReport`：RunId、状态、Reviewer、证据、Metrics、BuildVersion、ContentRevision。
- `UGamePlatformReviewSubsystem`：EditorSubsystem，创建待审报告并要求真实 Reviewer 显式确认。
- 自动化／AI 不允许代签 Passed；Passed 必须有证据。

这些类型继续作为统一人工审核底座。

## 2. 目标

每个重要插件无需启动完整《神兽联盟》，即可在最小依赖闭包下完成：

```text
准备
→ 自动前置校验
→ 加载可选Review场景
→ 运行插件Scenario
→ 收集自动Evidence/Metrics
→ 人工观察
→ 显式Reviewer结论
→ 输出Report
```

Review Harness 由 GamePlatformDeveloperTools 拥有，禁止各插件复制一套 Review Subsystem。

## 3. 后续推荐类型

### UGamePlatformReviewProfile

Editor-only DataAsset，建议字段：
- ProfileId。
- TargetPluginId。
- RequiredPluginIds。
- RequiredModuleNames。
- ReviewCase 软引用。
- OptionalReviewMap。
- RequiredDefinitions／Assets。
- TargetType：Logic／ClientVisual／Server／Network／Editor。
- Setup Policy／Teardown Policy。
- RequiredAutomationTests。
- RequiredEvidenceKinds。
- PerformanceMetricNames。

Review Profile 是开发资产，不进入 Shipping。

### IGamePlatformReviewScenarioProvider

仅在插件确需程序化 Setup/Teardown 时实现：
- Prepare。
- StartScenario。
- StopScenario。
- CollectEvidence。

不要求每个插件创建 Provider；纯数据/纯逻辑插件使用自动测试即可。

## 4. 插件类型与审核方式

### 纯逻辑

Core、Data、ApplicationFlow 等：
- Unit／Automation。
- Commandlet。
- 结构／依赖报告。
- 不强制 Review Map。

### 客户端视觉

UI、VFX、SFX、Animation、Camera：
- 必须真实 UE Review Map 或等价真实场景。
- 必须人工观察。
- 必须记录 BuildVersion／ContentRevision。

### 玩法／世界

Character、Combat、AI、Interaction、World、PCG：
- 最小 Review 场景。
- Dedicated Server 相关职责必须追加真实网络测试，不允许单机 Map 代替。

## 5. 证据

统一根目录：

```text
Game/Saved/GamePlatformValidation/<RunId>/Review/
├── review-summary.json
├── automation/
├── metrics/
├── screenshots/
└── reports/
```

不向 Source／Content 写运行证据。

## 6. G4 门禁

只有同时满足以下条件才允许 G4 Passed：
- 所需真实 `.uasset/.umap` 存在。
- 自动前置检查通过。
- 指定 Review Case 完成。
- RequiredEvidence 完整。
- Reviewer 非空。
- `bExplicitHumanConfirmation=true`。
- Report 对应当前 BuildVersion／ContentRevision。

当前工程 `.uasset/.umap=0`，因此任何需要真实内容的插件不得标 G4 Passed。

## 7. 自动化不可越权

自动化可以：
- 创建 Pending Report。
- 收集日志／截图／指标。
- 检查资产／配置／测试。

自动化或 AI 不可以：
- 填写真实 Reviewer。
- 把 NotRun 自动改为 Passed。
- 没有 Evidence 时宣告通过。

## 8. 后续接入顺序

1. 新增 ReviewProfile 类型和 Validator。
2. Commandlet 支持 `-ReviewProfile=<Id>` 的前置验证。
3. Editor UI 展示 Case／Evidence／Metrics。
4. VFX/UI/Combat 先作为三个试点。
5. 扩 Animation/Camera/SFX/AI/World/PCG。
6. 与 G0～G5 总门禁汇总，不把 Review 单独变成新的发布系统。

# WorldDirector（世界PCG编排器）

## Editor Validator（编辑器世界校验）

`UGamePlatformPCGWorldValidator（PCG世界编辑器校验器）`已加入 `GamePlatformPCGEditor（PCG编辑器模块）`源码，并显式依赖 `DataValidation（数据校验模块）`。规则：

1. 地图中没有任何 `AGamePlatformPCGActorBase（PCG放置器基类）`且没有 Director 时返回 NotValidated（不适用），普通非PCG地图不被强制污染。
2. 一旦存在平台 PCG 放置器，必须恰好一个 `AGamePlatformPCGWorldDirector`。
3. Director 自身参与者集合必须通过 `ValidateParticipantSet（验证参与者集合）`：同World、稳定SourceId、Schema主版本一致。
4. 地图中每一个真实 PCG 放置器必须显式注册到唯一 Director；禁止依赖运行时 `GetAllActorsOfClass（全世界Actor扫描）`补漏。
5. 该校验器当前代表源码已实现；在 UE5.8 Editor 模块编译并由 DataValidation 子系统真实调度前，不得写成“校验已通过”。

`AGamePlatformPCGWorldDirector（PCG世界编排器）`负责阶段级组织，不是第二套PCG调度器。

## 当前职责

- 显式注册/撤销 `AGamePlatformPCGActorBase（PCG放置器基类）`。
- 按 WorldStage（世界阶段）查询参与者。
- 校验参与者 World、SourceId（来源ID）和 Schema 主版本。
- 不执行全世界 Actor 扫描。
- 不复制 `UGamePlatformPCGWorldSubsystem（PCG世界子系统）`现有 Request/Cancel/Release（请求/取消/释放）生命周期。

当前关系：

```text
WorldDirector（阶段与参与者）
        ↓
GamePlatformPCGWorldSubsystem（执行生命周期/所有权）
        ↓
Official PCG（官方PCG）
```

## 使用方式

地编/项目组合层显式把放置器注册到每关唯一 Director。Editor Validator 源码已经检查 Director 数量、参与者集合合法性和地图中放置器是否完整注册。DirtyBounds（脏区域）、批次编排和正式阶段执行仍属于后续增量，不将尚未完成能力写成已交付。

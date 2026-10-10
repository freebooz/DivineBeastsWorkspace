# WorldDirector（世界PCG编排器）

## Editor Validator（编辑器世界校验）

`UGamePlatformPCGWorldValidator（PCG世界编辑器校验器）`已加入 `GamePlatformPCGEditor（PCG编辑器模块）`源码，并显式依赖 `DataValidation（数据校验模块）`。规则：

1. 地图中没有任何 `AGamePlatformPCGActorBase（PCG放置器基类）`且没有 Director 时返回 NotValidated（不适用），普通非PCG地图不被强制污染。
   - UE5.8实际编辑器日志显示：CanValidateAsset（是否接管资源）若误对所有UWorld返回true，再以NotValidated结束，会导致DataValidation记录“did not return a validation result”。现改为入口先审查已加载PCG放置器/Director及WorldPartition中的PCG ActorDesc（未加载描述符），**普通天气/前端地图在入口直接跳过**，含未加载PCG的地图继续失败关闭。最终编辑器回归仍须独立执行。
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
### P2～P6增量接口

- `CollectSpatialMasks（收集空间排除快照）`：从显式注册放置器读取道路样条、农田闭合地块、连接件、人工排除盒，输出最多256份确定性二维掩码；非法输入整体拒绝。
- `BuildStaticExecutionPlan（构造静态阶段计划）`：按固定WorldStage与SourceId排序，拒绝TerrainWrite/CutFill/Interiors/ApplyState/RuntimeDetail提前执行；**只是执行计划，不自动调用PCG生成或宣称Bake完成**。
- `CollectGameplayAnchorCandidates（收集玩法候选）`：仅消费已声明Play.*领域、属于GameplayAnchors阶段的参与者，并为世界/区域/修订派生稳定ID。服务器准入、出生、采集及存档必须通过项目已有权威系统。
- `UGamePlatformPCGWorldValidator（世界校验器）`增加WorldPartition ActorDesc（未加载Actor描述符）审查：发现未加载PCG放置器或Director时失败关闭，要求加载相关分区后重验。未加载分区的资产注册与Cook仍需要独立审计，静态检查不是全图通过证明。

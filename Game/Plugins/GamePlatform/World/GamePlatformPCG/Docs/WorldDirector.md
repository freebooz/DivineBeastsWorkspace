# WorldDirector（世界PCG编排器）

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

地编/项目组合层显式把放置器注册到每关唯一 Director。后续 Editor Validator 将检查 Director 数量和参与者重复身份。DirtyBounds（脏区域）、批次编排和正式阶段执行仍属于后续 M0/M1 增量，不将尚未完成能力写成已交付。

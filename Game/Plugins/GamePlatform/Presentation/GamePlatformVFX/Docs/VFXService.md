# VFXService（VFX服务）

`IGamePlatformVFXService` 由 `UGamePlatformVFXWorldSubsystem` 实现。每个 UWorld 独立持有 Instance Registry、Definition World Lease、预测去重和 Lifetime Timer；Catalog Registry 只保留旧工具兼容。

标准 `Play` 顺序：

```text
World/线程校验
→ Prediction去重/取消/纠正
→ Soft/Hard Budget
→ 取得Presentation已解析DefinitionId
→ GamePlatformData AcquireDefinition(World, VFXRuntime)
→ Definition + Parameter Schema校验
→ Niagara创建（不自动激活）
→ Instance登记 + OnSystemFinished绑定
→ Activate
```

`Stop`、自然 `OnSystemFinished`、Corrected/Cancelled、MaxLifetime 和 World Deinitialize 都统一释放 Definition Lease、Dedupe、Timer 和 Composite 子实例。

Gameplay 正常路径必须通过 `GamePlatformPresentation`，不直接链接 VFX Client。
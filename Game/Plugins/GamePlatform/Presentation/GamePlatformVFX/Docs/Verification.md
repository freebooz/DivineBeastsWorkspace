# Verification（验证与验收）

当前状态：P0 源码实现完成，仍处于 pending UE validation（待 UE 验证）。

必须完成的生产证据：
1. GamePlatformVFXClient 和 GamePlatformVFXEditor 的 UE5.8 编译通过。
2. 客户端 Cook/Stage 通过，Definition/Catalog/Niagara 资产依赖完整。
3. Dedicated Server 编译/Stage 不包含 VFX 客户端、编辑器模块和纯表现资源。
4. UE Automation（自动化测试）执行通过。
5. L_GPVFX_Review（人工核验地图）中完成 P0 五类效果的视觉、生命周期和质量档验证。
6. Unreal Insights / Profile（性能分析）确认实例数、Niagara 并发和 GPU 成本满足目标平台预算。

注意：工作区 Build/Manifests/engine.json 当前只声明 Unreal Engine 5.8，exactBuild（精确引擎构建）仍未锁定，因此正式生产验证前必须固定具体引擎构建版本。

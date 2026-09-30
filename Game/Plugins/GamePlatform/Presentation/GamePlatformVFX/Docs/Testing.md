# Testing（测试）

源码自动化测试覆盖：Resolver 精确解析、Catalog 注册/注销、Handle 有效性、Pending/Stop 生命周期、无效异步加载、Composite 默认行为、Presentation Provider 无 World 的确定性失败，以及通用 `User.*` Niagara 参数名稳定性。

工程脚本：
- Tests/Scripts/ValidateGamePlatformVFX.ps1：源码和目录边界静态检查。
- Tests/Scripts/CookGamePlatformVFXClient.ps1：调用 UE RunUAT 执行客户端 Build/Cook/Stage。
- Tests/Scripts/AuditGamePlatformVFXServerStage.ps1：检查 Dedicated Server Stage 中是否泄漏 VFX 客户端模块和已知纯表现资产。
- `ValidateGamePlatformVFX.ps1` 同时检查 VFX Lib 迁移的 Shader 目录、平台中立命名和 `SourceArt` / `Content` 边界。

这些测试不能替代实际 UE5.8 编译、Cook、Niagara 人工视觉 Review、GPU/Profile（性能分析）和联网场景测试。

# ServerAssetSafetyValidation（服务器资产安全验证）

UGamePlatformServerAssetSafetyValidator 以 /Server/ 或 /ServerSafe/ 资产为起点，通过 Asset Registry GetDependencies 读取磁盘依赖元数据。

默认禁止 Client、Presentation、UI、VFX、SFX、Cinematic、CommerceUI、Debug 等客户端路径，以及 Niagara、Widget、Sound、LevelSequence 等纯表现类。

失败证据输出 SourcePackage -> ForbiddenPackage（Class）。传递依赖的更完整链路由 Asset Audit/CI 图分析扩展。
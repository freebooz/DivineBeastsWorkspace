# ContentAndAssetValidation（内容与资产验证）

UGamePlatformContentValidator 检查 Redirector（重定向器）、长包路径合法性和 Developer 内容进入 Shipping 的风险。

FGamePlatformAssetAudit 使用 Asset Registry（资产注册表）元数据读取 Package、Class、PrimaryAssetType、ChunkIds、DiskSize，不默认加载全部 UObject。

Primary Asset、Chunk、Development/Editor Content Leak 的最终发布结论必须结合真实 Asset Registry 与 Cook Manifest。
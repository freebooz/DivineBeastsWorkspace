# AssetAuditAndReferenceAnalysis（资产审计与引用分析）

FGamePlatformAssetAudit 提供 AuditFolder、AuditDependencies、WriteCsv。

目标能力对应 Audit.Plugin、Audit.Folder、Audit.ServerCook、Audit.ClientCook、Audit.PrimaryAssets、Audit.Chunks、Audit.Dependencies。

审计默认基于 FAssetData/Registry 元数据，不自动删除资产，不无差别加载全部 UObject。输出格式支持 JSON、Markdown、CSV。
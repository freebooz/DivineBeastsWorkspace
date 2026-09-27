# Catalogs（语义目录）

UGamePlatformVFXCatalog（VFX目录资产）将 SemanticTag（语义标签）+ ContextId（上下文）解析为 Definition（定义资产）。

解析原则：
1. 精确语义优先于通用 Fallback（降级）。
2. 精确 Context 优先于 NAME_None 通用上下文。
3. Catalog Priority（目录优先级）高者优先。
4. Entry Priority（条目优先级）高者优先。
5. 分值相同时按 Definition 路径字典序稳定决策，避免注册顺序造成非确定结果。

Catalog 通过 FGamePlatformVFXRegistrationHandle（目录注册句柄）注册和注销。项目层、英雄包和世界包可以叠加目录，但平台默认 Catalog 不得写入 DBA.* 或 MOBA 项目语义。

# CatalogResolution（VFX目录解析）

Resolver使用确定性排序：DefinitionId直达或Semantic exact → Semantic parent fallback → Context → Specificity → Scope → Priority。平台/质量和Required/Blocked ContextTags先过滤。

同一最终Rank出现多个候选时设置bAmbiguous并Fail Closed，不依赖数组、加载、注册、Hash或资产路径顺序。Cache键包含Revision、Semantic、DefinitionId、Context、ContextTags、Platform、Quality、Fallback；Catalog变化后失效。
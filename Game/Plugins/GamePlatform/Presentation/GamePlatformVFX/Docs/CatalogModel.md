# CatalogModel（VFX目录模型）

UGamePlatformVFXCatalog支持Fragment动态注册/注销。Entry包含SemanticTag、ContextId、Required/Blocked ContextTags、逻辑DefinitionId、Definition软引用、Priority、Specificity、Scope、PlatformId、QualityTier和Fallback。

Scope为 Platform < Shared < Project < ContentPack；GamePlatformVFX本身不依赖上层。注册/注销会推进Registry Revision并使Resolver Cache失效，重复注册同一Catalog对象被拒绝。
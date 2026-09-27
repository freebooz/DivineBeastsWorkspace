# CatalogAndResolutionBoundary（目录与解析边界）

MobaPresentation只提交Semantic（语义）和Context（上下文），不决定最终Catalog（资源目录）条目。

资源解析优先级由具体Presentation Provider（表现提供者）及Catalog系统负责，可按 ContentPack > Project > Moba > Platform（内容包 > 项目 > MOBA > 平台）实现，但本插件不读取具体VFX/SFX路径。

禁止依赖插件加载顺序、Hash顺序或资产扫描顺序做语义选择。同优先级冲突应由DeveloperTools（开发者工具）验证失败。

第一版没有MOBA默认资源，因此不创建伪造的Content/Defaults二进制资产。

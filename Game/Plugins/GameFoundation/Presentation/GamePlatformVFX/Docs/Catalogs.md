# Catalogs（目录与解析）

Resolver 规则：

1. `ExplicitDefinitionId` 直接命中。
2. Semantic 精确匹配优先。
3. Required / Blocked Context 过滤。
4. `ContentPack > Project > Moba > Platform`。
5. Context Specificity（具体度）越高越优先。
6. Explicit Priority（显式优先级）。
7. 允许时进行父语义回退。
8. 同分但 Definition 不同视为歧义，运行时拒绝；编辑器验证应提前报错。

不得依赖插件加载顺序、数组顺序或哈希遍历顺序。

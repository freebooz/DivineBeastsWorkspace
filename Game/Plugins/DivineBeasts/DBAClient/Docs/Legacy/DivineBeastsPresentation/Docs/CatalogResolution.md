# CatalogResolution（目录解析）

GamePlatformPresentation的中立Resolver按确定性顺序解析：

1. Scope precedence（作用域优先）：ContentPack > Project > Moba > Platform。
2. Semantic exact优于parent fallback。
3. ContextQuery必须匹配。
4. Specificity（具体度）更高优先。
5. Priority（优先级）更高优先。

同一最高评分存在不同EntryId时返回Ambiguous（歧义），禁止按注册顺序、数组顺序、Hash顺序或扫描顺序选择。

平台Automation源码覆盖ContentPack覆盖Project、同级歧义失败、Parent fallback和Context RejectConflict；真实UE Automation运行状态仍为“未执行”。

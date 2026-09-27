# QueryFiltersAndAreas（查询过滤器与导航区域）

公共 Request 只传 `FilterId`。服务器 Registry 确定性映射到本地可信 `UNavigationQueryFilter`类；空 ID使用 `Navigation.Filter.Default`。重复 FilterId 注册失败。

第一版跨游戏 Area 只有 `UGamePlatformNavArea_Default`、`UGamePlatformNavArea_HighCost`、`UGamePlatformNavArea_Blocked`。HighCost 的默认成本来自配置；Blocked 继承 `UNavArea_Null`。

`UGamePlatformNavigationQueryFilter_Default`作为默认过滤器，NavData 使用自身 QueryFilter Cache（查询过滤器缓存）。实际项目未来可注册更多稳定 ID，但不得由客户端上传任意 Class Path（类路径）。

真实“高成本区域绕行”和“Filter 禁用区域”需要 NavModifier/地图资产后执行；当前只能认定接口和服务器映射源码已实现。

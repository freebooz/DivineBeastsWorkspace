# QueryFiltersAndAreas（查询过滤器与导航区域）

公共 Request 只传 `FilterId`。服务器 Registry 确定性映射到本地可信 `UNavigationQueryFilter`类；空 ID使用 `Navigation.Filter.Default`。重复 FilterId 注册失败。

第一版跨游戏 Area 只有 `UGamePlatformNavArea_Default`、`UGamePlatformNavArea_HighCost`、`UGamePlatformNavArea_Blocked`。HighCost 的默认成本来自配置；Blocked 继承 `UNavArea_Null`。

`UGamePlatformNavigationQueryFilter_Default`作为默认过滤器，通过`UNavigationQueryFilter::GetQueryFilter`在首次查询时初始化，再由当前NavData持有自身 QueryFilter Cache（查询过滤器缓存）。`ANavigationData::GetQueryFilter`仅读取缓存，不能作为冷启动的创建入口。注册但未使用过的过滤器应当正常初始化；未知FilterId仍明确拒绝，不回退成默认过滤器。实际项目未来可注册更多稳定 ID，但不得由客户端上传任意 Class Path（类路径）。

`GamePlatform.Navigation.Server.ColdFilterInitializesBeforeQuery`使用真实UWorld、NavigationSystem和无瓦片NavData调用平台随机可达点/投影入口，覆盖空缓存、默认ID别名、缓存复用、未知代理/过滤器拒绝和再次冷初始化。此夹具验证过滤器创建及错误分层，不验证地图可达点或怪物实际位移；执行结果须以本轮UE Automation报告为准。

真实“高成本区域绕行”和“Filter 禁用区域”需要 NavModifier/地图资产后执行；当前只能认定接口和服务器映射源码已实现。

2026-10-18真实Development Server验证：`ColdFilterInitializesBeforeQuery` 已增加ServerContext并实际执行Success；同批AI生命周期测试Success，合计2项、0失败、进程退出0。报告 `Saved/Validation/FoundationM0/22298c1f-5339-4607-918b-1b03031ed463/NavigationServerRegression/Report/index.json`。打包环境缺少Automation HTML模板，但JSON实际写入并核对测试名称、数量、状态；不将HTML缺失包装为完整可视报告。另用真实Village地图完成10/10怪物位移与后续巡逻点验收，见 `Saved/Validation/SkillBar20261018/PatrolAfterNavigationFix.json`。

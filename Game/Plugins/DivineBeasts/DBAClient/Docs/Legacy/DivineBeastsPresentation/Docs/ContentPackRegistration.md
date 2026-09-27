# ContentPackRegistration（内容包注册）

FDivineBeastsPresentationContentPackFragment定义ContentPackId、Revision、LifecycleScope、CatalogFragment、LogicalPreloadDefinitionIds和Required/Optional预加载提示。

Activate流程：

Validate → Register Catalog Fragment → 返回项目Pack Handle → 可选发出Logical Preload Request。

Deactivate流程：

Cancel dependent logical preload → Unregister Catalog Handle → 删除Pack Handle。

同一ContentPackId重复激活会拒绝，Revision/OwnerScope/LifecycleScope不一致会验证失败，Stale Handle（过期句柄）重复注销安全失败。

当前没有真实DBAPresentationPack/DBAHeroPack/DBAWorldPack/DBASkinPack，因此真实包注册运行测试为“未执行”。

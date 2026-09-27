# TestingAndEvidence（测试与证据）

源码Automation测试覆盖：
- 五ArenaMode存在且人数正确。
- TeamCount=2。
- MainArena Role/Experience。
- HeroCatalogRevision一致。
- Structural Validation通过。
- Production Validation在规则未批准时失败。
- FoundationTest Assignment不能穿过项目Production Gate。

PowerShell专项测试覆盖：
- Runtime/Client/Server Module Gate。
- 主工程DivineBeastsArena模块名冲突保护。
- 五模式与单Server Target/Binary。
- 12 Hero Catalog与资格Fail Closed。
- Assignment/TransferTicket/CharacterId链。
- MatchResult不再把HeroDefinitionId写入CharacterId。
- PostMatch重新请求OpenWorld。
- 无新Arena Go微服务。

当前真实UE Automation、Go test/vet/race、Dedicated Server、Cook、性能均未执行。静态检查不能替代这些证据。

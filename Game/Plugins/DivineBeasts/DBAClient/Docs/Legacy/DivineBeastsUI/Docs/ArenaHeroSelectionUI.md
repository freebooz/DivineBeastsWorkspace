# ArenaHeroSelectionUI（竞技英雄选择界面）

Arena Hero Selection（竞技英雄选择）使用 HeroDefinitionId，不使用持久CharacterId替代Hero模板身份。

Eligibility（资格）最终由服务器/可信Provider决定。UI只应该显示可选/禁用原因，不自行判定权益。

当前工作树没有Arena Client Hero Selection提交端口，因此：

- bHeroSelectionCommandAvailable=false。
- ArenaSelectHero / ArenaReady命令返回 Arena.ClientCommandPortUnavailable。
- 页面工程接口存在，但真实提交运行状态为“未执行”。

未批准Pick/Ban时不创建选禁流程；未批准DuplicateHero策略时不自动禁止重复Hero。

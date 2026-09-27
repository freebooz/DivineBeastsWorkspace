# DivineBeastsArena（神兽联盟项目竞技插件）

DivineBeastsArena位于DivineBeasts/Gameplay（神兽联盟项目玩法扩展层），是一期必建项目竞技插件。

模块：
- DivineBeastsArenaRuntime（项目竞技运行模块）：Runtime（双端），五模式结构、Production配置门禁、项目Arena Definition和可信Hero资格接口。
- DivineBeastsArenaClient（项目竞技客户端适配模块）：ClientOnly（仅客户端），通过Module Gate后创建，负责项目模式元数据、ApplicationFlow竞技扩展和PostMatch返回。
- DivineBeastsArenaServer（项目竞技服务器扩展模块）：ServerOnly（仅服务器），通过Module Gate后创建，负责Production Assignment/Revision/Hero资格Fail Closed门禁。

主工程模块已经叫DivineBeastsArena，因此插件内没有同名C++模块。

正式五模式固定为Arena.Mode.Duel1v1、Team2v2、Team3v3、Team4v4、Team5v5，TeamCount=2，TeamSize=1~5，TotalPlayers=2/4/6/8/10；全部使用GameServer.Role.MainArena + Experience.MainArena.Main + 同一个DivineBeastsArenaServer Target/Server Binary。

当前产品未批准MapId、TimeLimit、Score/Win、Respawn、Overtime/SuddenDeath、Pick/Ban、DuplicateHero等正式值，因此Project Catalog明确保持NotConfigured，Production Release Gate当前为失败/阻塞。平台FoundationTest默认值不会被当作项目Production规则。

新增Go微服务：无。新增Go业务接口：无。MatchService/GameServerControl/MatchResult继续复用现有后端。

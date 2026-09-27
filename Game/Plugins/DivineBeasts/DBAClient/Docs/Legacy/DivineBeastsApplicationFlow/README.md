# DivineBeastsApplicationFlow（神兽联盟应用流程插件）

本插件位于 DivineBeasts/Application（神兽联盟项目流程装配层），唯一可加载模块为 DivineBeastsApplicationFlowClient（项目客户端应用流程模块，ClientOnly）。

它不创建第二套流程框架，而是注册项目节点并复用 GamePlatformApplicationFlow（游戏平台应用流程）提供的 UGameInstanceSubsystem（游戏实例子系统）、FlowRunId（流程运行编号）、NodeGeneration（节点世代）和 OperationToken（操作关联令牌）。

第一版闭环为 Boot→Initialize→Authentication→LoadProfile→LoadRoster→CharacterEntry→Create/Select Persistent Character→ValidateSelection→RequestWorld→TransferWorld→WorldReady→InWorld。世界入口仅支持 OpenWorld.Hub/Main 和 Village.Main/Tutorial/Training；MainArena 由可选扩展组合接入。

客户端不决定具体服务器，不直接调用 ClientTravel，不记录 TransferTicket（迁移票据）原文，不保存密码或Token。新增Go微服务：无；正式Go入口仍为5个。

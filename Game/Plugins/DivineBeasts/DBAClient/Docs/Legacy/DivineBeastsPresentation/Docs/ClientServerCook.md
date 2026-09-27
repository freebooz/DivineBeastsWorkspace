# ClientServerCook（客户端/服务器烘焙）

Client Cook（客户端烘焙）应包含实际Project/ContentPack表现覆盖以及对应Provider资源，但不得包含后端Secret。

Server Cook（服务器烘焙）允许DivineBeastsPresentationRuntime中立类型/Tag，不应包含DivineBeastsPresentationClient、Niagara、SoundWave、WidgetBlueprint、Camera资源或纯客户端DBAPresentationPack/HeroPack/WorldPack/SkinPack资源。

本轮只完成源码Server-safe静态门禁；没有真实Client/Server Cook工件，所以两端Cook均为“未执行”。

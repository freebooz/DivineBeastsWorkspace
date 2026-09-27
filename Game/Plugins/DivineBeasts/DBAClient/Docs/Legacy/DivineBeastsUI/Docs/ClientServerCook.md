# ClientServerCook（客户端/服务器烘焙）

DivineBeastsUI.uplugin只有 DivineBeastsUIClient（ClientOnly）模块。

DivineBeastsArenaServer.Target.cs（统一服务器构建目标）显式 DisablePlugins.Add("DivineBeastsUI")，避免Dedicated Server包含项目UI插件。

Server Cook（服务器烘焙）不得包含：

- DivineBeastsUIClient
- /DivineBeastsUI/ Widget资产
- CommonUI项目资源
- DBAUIPack_Core
- UI icon/portrait（图标/头像）

当前真实UI二进制资产=0，Client/Server Cook工件也未提供，因此真实Client Cook与Server Cook状态均为“未执行”。静态ClientOnly/Server Target隔离通过不能替代Cook证据。

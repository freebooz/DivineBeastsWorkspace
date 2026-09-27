# ContentPackOwnership（内容包所有权）

GamePlatformVFX只拥有中立Defaults、Examples、Review和TestAssets。项目VFX应位于DivineBeasts内容包；MOBA复用层可提供共享Fragment，但平台插件不反向依赖。

Resolver的Scope支持 Platform < Shared < Project < ContentPack 的覆盖关系，Catalog Fragment可以动态注册/注销并触发Revision缓存失效。

GamePlatformVFX本身不扫描上层目录来建立隐式依赖；上层Composition Root负责注册其Catalog Fragment。
# DedicatedServerSafety（专用服务器安全）

Dedicated Server只需要DivineBeastsPresentationRuntime的中立Context/Catalog/Tag/事实类型；DivineBeastsPresentationClient为ClientOnly，不应进入Server模块。

Runtime源码静态门禁禁止Niagara、Sound、Widget、Camera客户端执行类型和VFX/SFX/UI/Animation/Camera Client模块依赖。

Server Cook还应验证DBAPresentationPack_Core、DBAHeroPack_*、DBAWorldPack_*、DBASkinPack_*等纯表现资源不泄漏。当前没有真实Server Cook工件，因此Cook状态为“未执行”。

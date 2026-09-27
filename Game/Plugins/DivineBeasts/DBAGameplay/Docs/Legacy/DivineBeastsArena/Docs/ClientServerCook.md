# ClientServerCook（客户端与服务器烘焙）

Client需要：
- Runtime项目Mode元数据。
- Client ApplicationFlow适配。
- 通用Arena客户端ViewModel/请求类型。
- 客户端需要的Hero/Map表现内容。

Server需要：
- Runtime项目Definition/Revision。
- Server Project Extension。
- Hero Definition Server-safe数据。
- MainArena gameplay geometry/collision/spawn/objective actors。

Server禁止：
- UI。
- Niagara。
- SFX。
- 高精纯表现资产。
- ClientOnly模块。

当前没有真实Client/Server Cook工件，因此Cook均为未执行；源码边界通过不能替代Cook报告。

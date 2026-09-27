# BackendBoundary（后端边界）

GamePlatformVFX不直接访问HTTP、Gateway、PlayerData、MatchService或任何Go业务服务。逐VFX调用后端被禁止。

服务端/业务系统只产生Gameplay Fact或中立Presentation Request，客户端VFX负责解析并渲染。

新增Go业务后端接口：不适用。
新增Go微服务：不适用。
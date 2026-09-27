# ClientServerLeakValidation（客户端/服务器泄漏验证）

UGamePlatformClientLeakValidator 与 UGamePlatformServerLeakValidator 只对真实 Artifact Root（构建/Cook 工件目录）给出端侧结论。

Client 禁止已知 ServerOnly 模块；Server 禁止 ClientOnly、DebugClient、CommerceUI、DBAClient 和 DeveloperTools。

Build/Validation/ValidateCook.ps1 同步检查工件文件、manifest、receipt 等文本清单。

没有 Client receipt、Server receipt、Cook manifest 或真实包目录时状态必须是 未执行，不能根据源码目录推断通过。
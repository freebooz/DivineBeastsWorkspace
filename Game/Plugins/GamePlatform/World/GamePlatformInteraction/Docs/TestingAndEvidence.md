# TestingAndEvidence（测试与证据）

C++ Automation 测试源码当前 3 个：InteractionOption 结构验证、Focus 稳定排序/距离过滤、Interactor/Interactable 组件与 Request 身份字段构造。

`Build/Validation/VerifyInteraction.ps1`已执行静态门禁，确认单 Runtime 模块、允许依赖、禁止上层/客户端模块依赖、Request 不含服务器权威字段、仅两个低频 Server RPC、Development Door/Pickup/Harvest 胶水存在，二进制资产数量为0。

全工作区插件分层验证在目录迁移/清单同步后执行通过：44插件、76模块、18条项目内依赖边、739项检查。最终源码完成后仍会再执行一次门禁。

UE Automation 实际运行、Multi-PIE、Dedicated Server 双客户端、Late Join、网络延迟/断线、三目标构建和 Client/Server Cook 尚未执行。Network验证入口已实际运行并返回 `not_executed`，原因是 `UE_ROOT/UnrealEditor-Cmd.exe unavailable`；Cook入口启动前被工具侧安全检查拦截，随后只读前置检查确认 `UE_ROOT`为空且 `RunUAT.bat`不存在，因此 Cook 同样保持未执行。

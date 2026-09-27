# DivineBeastsRuntime（神兽联盟运行时核心插件）

DivineBeastsRuntime位于DivineBeasts（神兽联盟项目层）/Core（项目核心），是项目层第一个轻量双端运行时插件。正式名称固定为DivineBeastsRuntime，不使用DivineBeastsCore。

本插件只负责：项目身份、项目稳定ID允许值、ServerRole/Experience/ArenaMode映射、FDivineBeastsProjectContext（项目上下文）、ContractVersion（契约版本）与Compatibility（兼容摘要）查询，以及Shared Generated C++（共享生成C++）的运行时适配。

加载模块只有DivineBeastsRuntime（运行时模块）。DivineBeastsContracts（项目共享协议外部库模块）使用ModuleType.External（外部模块类型），只描述生成头和静态库链接方式，不写入.uplugin Modules数组。

跨语言真源固定为Shared/Contracts/Games/DivineBeasts。正式GameId为divinebeasts，ProjectId为DivineBeastsArena；这是本项目首次在Shared真源中正式确定项目级GameId，不存在另一份旧正式GameId与之冲突。

本插件不实现Characters、Abilities、Combat、Arena规则、World行为、Presentation、UI、VFX、SFX、HTTP、gRPC业务调用、数据库或后端服务。新增Go业务后端接口：无。

当前源码/契约/生成一致性可以静态验证；当前Runner缺少UE5.8、Go、MSVC工具链，因此UE Build、Cook、Go test和External真实链接必须按证据标记为“未执行”。

# DBAArena（神兽联盟可选竞技插件）

正式位置：`Game/Plugins/DivineBeasts/DBAArena/`。本插件承接原本分散在DBAGameplay、DBAClient和DBAServer中的项目竞技适配，保留既有模块、反射类型、导出宏和五种竞技模式身份；不复制通用竞技机制。

| 模块 | 目标 | 职责 |
| --- | --- | --- |
| DivineBeastsArenaRuntime | 双端／编辑器 | 项目竞技模式、资格与规则配置 |
| DivineBeastsArenaClient | Client／Editor | 匹配请求、公开流程扩展、赛后返回入口 |
| DivineBeastsArenaServer | Server／Editor | MainArena项目权威适配 |

## 依赖与装配

- 公共依赖为DBAGameplay和MobaCommon/GamePlatformArena。项目公共玩法、世界、客户端和服务器插件不反向依赖本插件。
- 客户端模块在私有构建依赖中使用DBAClient的`IDivineBeastsApplicationFlowExtension`与公开流程服务；既有按GameInstance注册／注销机制保留，不创建全局桥接器或第二套流程执行器。插件对DBAClient的引用只允许Client／Editor目标。
- Server目标不启用DBAClient，不编译DivineBeastsArenaClient。三角色仍共用一个Server Target；运行角色为MainArena时使用竞技扩展，不能按1v1至5v5再拆程序。
- MobaPresentation是独立MOBA语义插件，按客户端产品组合显式选择；公共项目表现不能硬依赖它，本次不伪称新增了未实现的表现接线。
- 主工程现有Foundation验证装配未被擅自改为自动启动全部业务子系统。关闭竞技验证使用同一正式工程的插件选择，不创建新DevHost；完整UE启用、编译、启动验收仍需实际工具链。

## 生命周期与内容

竞技客户端注册公开流程扩展，反初始化时注销、释放扩展引用及世界状态。服务调用、资产失败与角色就绪沿用既有明确错误路径；不因目录迁移增加固定成功或模拟准入。

项目竞技场地图、美术及专属内容的目标所有者为`ContentPacks/Worlds/DBAWorldPack_MainArena`。本插件只保留代码与必要项目模式定义，不复制地图或通用VFX执行器。内容包尚未交付，不创建假资产。

## 验证

- `Tests/Architecture/DBAPluginConsolidation.Tests.ps1`：模块唯一归属与声明。
- `Tests/Architecture/PluginCompositionAudit.Tests.ps1`：禁用竞技后的公共闭包、竞技Server不引入客户端、循环与隐藏依赖负例。
- `Tests/Architecture/ValidateDesignBaseline.ps1`：46个代码／机制插件＋登记内容插件。
- UE测试源码仍位于各模块的`Private/Tests`。结构检查不是UE编译、Cook、网络或五模式运行验收；本轮证据见总体实施规划。

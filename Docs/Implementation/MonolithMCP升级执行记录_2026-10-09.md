# Monolith MCP v0.23.0 全局与项目升级执行记录

2026-10-09用户明确授权升级全局和本项目Monolith MCP到v0.23.0。本文记录真实安装、连接、锁定引擎兼容及回退边界；进行中的步骤不能作为工具已升级可用或资产验收通过的证据。

## 来源与实际安装范围

官方源为tumourlove/monolith的v0.23.0发布，选择Monolith-v0.23.0-UE5.8.zip；发布页校验标记与下载文件均为SHA-256 `550be7b4e5f232bfca0f5800df7e917836fb405b53af02664f1b359405191d89`。只使用该发布源码，不把master后续提交冒充v0.23.0。

当前机器引擎级安装位于`F:/UnrealEngine-5.8.0-release/Engine/Plugins/Marketplace/Monolith`。这是本次环境证据，工程构建仍按已有EngineRoot解析方式运行；不会把个人路径写成新的通用构建规则。升级前插件描述和源码宏均为0.22.0。

Codex全局config.toml的`monolith`与`-monolith-`连接项指向该安装的同一个Binaries/monolith_proxy.exe。正式项目Game/DivineBeastsArena.uproject启用引擎级Monolith且只允许Editor目标。没有项目内另一个Monolith实现，也未新建同名插件或扩展46个自有代码/机制插件基线；两项连接及项目均消费同一份升级后的安装。

现有代理exe与官方v0.23.0包完全同字节，SHA-256均为`37c69776fea8edf715a7a389eb54024b7bcff1a7d7e340d1aa8d2b972d4910cc`。保留已经满足新包身份的在用代理，避免中断其他已连接stdio会话；离线monolith_query.exe的版本字节确有变化，随新包更新。exe没有Windows版本资源，不用文件日期推定语义版本。

## 锁定工具链与真实编译

项目锁定UE5.8.0源码引擎，实际BuildId为`cf41249f-44dc-4956-bd09-4a42f079e86b`；发布包UnrealEditor.modules的BuildId为`55116800`，不能手改索引把原二进制冒充兼容。v0.23.0源码须经锁定引擎重新构建，生成真实模块索引与DLL后再安装。

锁定引擎的BuildPluginCommand.Automation.cs不转发UsePrecompiled/MaxParallelActions，因此采用其等价原生ForeignPlugin构建：在Saved临时HostProject复制官方源码，官方UBT显式-Plugin/-Manifest/-UsePrecompiled/-MaxParallelActions=2真实编译20个Editor宿主模块（19个Editor模块和MonolithAudioRuntime），引擎依赖保持已有预编译产物。UEBuildTarget的ForeignPlugin分支只对该插件清除bUsePrecompiled并执行precompile；不构建另一份正式游戏插件或修改第三方源码。

本项目Monolith仅Editor启用；该构建不宣称UnrealGame Development/Shipping、服务器、其他平台、完整引擎源码或Cook通过。v0.23.0发布说明记录了源码中的一处5.8弃用告警和master后续修正，实际构建结果保留，不静默换分支、降低引擎或禁用模块。

## 备份、切换与回退

完整旧安装备份位于Saved/ToolUpdates/Monolith-0.23.0/Backup/Monolith-0.22.0，位于插件扫描根之外，1393个文件、459793976字节均已逐文件核对。旧全局配置备份保留在用户.codex/backups/monolith-0.23.0-20261009目录，不复制潜在敏感全局配置到版本库；正式uproject原文件另保存在Saved。Saved中的日志、下载、备份、HostProject和编译产物均为瞬态工具交付材料，不提交为第三方源码。

切换前本任务Editor已确认没有图标资产修改、没有未保存用户包，按PID、工程、启动时间与本任务日志关闭；不按名称终止无关Editor或构建。只允许替换经过绝对路径白名单检查的Monolith安装内容，保持同一全局连接路径。删除前还核当前旧安装与备份完全一致，保护备份后他人修改；安装/回读失败自动恢复旧安装并校验，回退失败单独留证且禁止继续启动。切换脚本经独立只读复核，原两项Important已修，最终范围未发现Critical/Important；该源码审查不冒充故障回退演练。

## 当前验证边界

官方包SHA校验一致；锁定UE5.8.0 Win64 Development真实ForeignPlugin构建80动作、358.69秒、退出0，20个DLL齐全。原生BuildId与锁定引擎一致；Staging、Host及最终Package中的732个官方源码/描述文件均与原始SHA一致。发布标签材质源码的C4996真实记录为一条warning，材质模块仍成功链接，未手改第三方源码。

最终候选含985文件、1509627453字节，保留官方两CLI并排除异BuildId的官方Win64 UE DLL。切换脚本真实退出0，安装985文件逐一回读长度与SHA一致；全局monolith和-monolith-两连接、本项目Editor专用启用项均核对有效，未改全局配置或为升级改项目描述。实际证据为CompiledForLockedEngine/Evidence/EditorCompileResult.json、PackageVerification.json、PackageCliToolVerification.json、InstallResult.json及IntegrationVerification.json。

正式Game/DivineBeastsArena.uproject已用锁定Editor启动，PID18572与参数保存在EditorLaunch.json。两项全局连接的实际monolith_status均回读0.23.0、server_running=true、port9316、1355个动作/26个命名空间；通过Monolith.editor.run_python回读正式工程绝对路径及5.8.0-0+UE5身份一致，UI导入动作已复核。v0.23新增input的13动作与localization的4动作已在真实服务中发现，未把发布说明预估的1400+数量冒充本机启用数量。

初次启动经历原生资源索引、骨骼网格编译与自动关闭的慢任务窗口，期间代理健康查询超时；未据此改装旧版或强行替换资产。Main启动记录有13条无具体测试名的ScriptStruct初始化条件错误，未运行Automation，不将服务健康表述为全Editor零错误或全部游戏类型通过。具体启动日志完整保留。

运行身份与UI动作目录证据为Saved/Validation/ZodiacSkillIcons-2026-10-09/MonolithAuthoringIdentity.json；另一全局连接和新增命名空间回读在本次工具升级目录GlobalSecondaryConnectionStatus.json、InputNamespaceDiscovery.json、LocalizationNamespaceDiscovery.json。全局与项目的0.23.0安装及运行验证已完成，60枚技能纹理导入/保存/原生重载另按逐包清单记录。工具连通不代替Widget/HUD、游戏运行、网络、Cook或设备视觉验收。

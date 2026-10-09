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

完整旧安装备份位于Saved/ToolUpdates/Monolith-0.23.0/Backup/Monolith-0.22.0，位于插件扫描根之外，逐文件校验后才允许替换。旧全局配置备份保留在用户.codex/backups/monolith-0.23.0-20261009目录，不复制潜在敏感全局配置到版本库；正式uproject原文件另保存在Saved。Saved中的日志、下载、备份、HostProject和编译产物均为瞬态工具交付材料，不提交为第三方源码。

切换前本任务Editor已确认没有图标资产修改、没有未保存用户包，按PID、工程、启动时间与本任务日志关闭；不按名称终止无关Editor或构建。只允许替换经过绝对路径白名单检查的Monolith安装内容，保持同一全局连接路径。失败时恢复经校验的旧源码/DLL/配置，再启动原引擎；不删除用户资产、原PNG或其他项目修改。

## 当前验证边界

官方包下载与SHA校验已执行并一致，旧安装/配置备份和锁定SDK源码构建正在执行；当前尚未宣称安装切换、服务0.23.0回读、动作目录或资产生成通过。完整实际命令、退出码、产物BuildId及安装校验将归档到Saved/ToolUpdates/Monolith-0.23.0，并在完成后同步本文。

升级后须重新启动正式工程，通过monolith_status读实际服务版本，读取项目文件路径和引擎版本，检查UI原生动作目录；随后继续60枚生肖技能纹理的Monolith导入、保存与原生包重载。工具连通不代替Widget/HUD、游戏运行、网络、Cook或设备视觉验收。

# TAB目标切换执行计划

用户确认：循环全部可选择单位，包括友方。沿用当前世界控制器的鼠标选择合同，100米范围，跳过自身、死亡、隐藏、离开世界和被遮挡的对象。每次按下按距离、稳定对象路径排序，选择当前项之后的一项并循环；没有候选时清除旧选择。只在按键事件查询，不加Tick或计时器。

架构：DefaultInput.ini登记单一DBA.CycleWorldTarget动作；DBAWorldsRuntime拥有当前新手村/开放世界选择输入；同一次SetSelectedWorldTarget更新既有UI通知和平台Combat可靠目标请求。交互距离、施法资格、投射物原目标冻结继续由原有服务器合同决定。GameAndUI先交给UI，暂停或世界未Active时不得切换目标；不修改Widget资产。

- [x] 先添加原生回归；预实现运行被外部DLL占用阻断，未把该阶段记为通过。最终回归已真实执行。
- [x] 接入按键、按需候选过滤与循环；验证友方、循环、死亡/遮挡/距离、销毁和暂停清理。
- [x] 同参数完整增量Editor/Client/Server构建，执行受影响原生回归。
- [ ] 双端Cook/Stage及真实Village/MainArena版本门禁后更新两个验证客户端；人工联机单列。

范围说明：当前目标选中真源为ADivineBeastsWorldPlayerController；独立GamePlatformArenaPlayerController没有这条鼠标选择链，属于后续竞技目标交互集成范围，不能把本次世界控制器测试当作竞技场人工验收。

执行记录：初次Editor构建的测试CPP已编译，链接被另一个`-OrbV2ReworkOwned`编辑器PID53056占用阻断（LNK1104），未将此构建或未运行测试标为通过。按用户此前“暂时无法暂停，先完成源码和工具”的约束保留该编辑器；先完成源码与测试，完整原生回归等待DLL释放。

验证补充：最终使用既有VillageDevelopmentClient开发配置、DBADevelopmentSkills及textconfig启动参数；真实LocalPlayer夹具已补齐。TAB/火冰起手与中断/动画共20项原生测试成功，现有世界操控静态门禁通过。此前16项失败为漏开发配置及本地玩家夹具，不记通过；并行技能栏源码改动后的完整Editor已重新构建。最终交付记录见Saved/Validation/WorldControlsCastingLighting20261025。

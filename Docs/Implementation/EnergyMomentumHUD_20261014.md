# SSLM 生命、能量与气势三独立状态条实施验收记录

日期：2026-10-14；工程：`DivineBeastsWorkspace/Game/DivineBeastsArena.uproject`（SSLM主工程），UE5.8（虚幻引擎）。

## 新需求与归属

用户要求“血条下面的蓝条显示能量，气势另外显示”。现三条在战斗状态面板中自上而下排列：Health/MaxHealth（生命/上限，红色）、Energy/MaxEnergy（独立能量/上限，蓝色）、Momentum/MaxMomentum（独立气势/上限，金色）。`GamePlatformCombat`（平台战斗）持有前两组GAS（玩法能力系统）真值，其中新建能量默认100/100，服务器权威、COND_OwnerOnly（仅拥有者）复制；`DBAGameplay`（SSLM项目玩法）仍持有气势，默认0/100。界面不把气势冒充能量、不自行计算/写入权威属性，也不重新启用旧五行与共鸣规则。能量的消耗/自动恢复机制本轮未经批准，故目前仅提供独立基础数值与新化身重置补满；原有火焰弹/寒冰箭/奥术飞弹继续消耗项目气势。

此需求将精简GAS模型由**2组8字段**扩展为**2组10字段**：Health、MaxHealth属于按网络相关性复制的公开数值；Energy、MaxEnergy、DamageBonus、DamageReduction、Momentum、MaxMomentum共6项仅拥有者复制；IncomingDamage、IncomingHealing两项元属性不复制。平台属性集仍只有`UGamePlatformCombatAttributeSet`（统一战斗属性集），没有独立第三份资源属性集，`UDivineBeastsMomentumAttributeSet`（项目气势属性集）继续位于第三层。

## 源码落地

1. `GamePlatformCombatAttributeSet`（平台统一战斗属性集）：增加Energy/MaxEnergy及GAS访问器、RepNotify（复制通知）、PreAttributeChange（数值越界裁剪）与PostGameplayEffectExecute（效果执行后校正），初始100/100。`GamePlatformCombatComponent::ResetForNewAvatar`（化身重置）补满能量，不恢复/消耗气势。
2. `FDivineBeastsPlayerStatusViewData`（玩家状态只读投影）、`UDivineBeastsPlayerStatusViewModel`（状态模型）：分别从平台Combat属性集读取Energy和MaxEnergy，并注册/注销两个对应GAS属性变化事件。Momentum单独从项目Momentum属性集读取。
3. `UDivineBeastsPlayerStatusPanel`（战斗状态面板）：有HealthBar、EnergyBar、MomentumBar三个可选命名蓝图变量，以独立ResourceId（资源标识）刷新资源条，蓝色能量`(0.14,0.43,0.96)`、金色气势`(0.97,0.69,0.22)`，与旧ShieldBar（盾条）无关。`GamePlatformResourceBarWidget::SetResourceFillColor`（通用资源条设置填充颜色）只改变UI，不获取GAS数据。
4. 新增/扩展原生回归：`GamePlatform.Combat.Attributes.DefaultsAndClamp`（能量默认及裁剪）和`DivineBeasts.UI.Combat.PlayerStatusSnapshot`（65点能量与40点气势的独立读取）。`Tests/Architecture/ValidateCompactGASAttributes.py`（GAS简化模型门禁）核查2组10字段；`Tests/Architecture/ValidateEnergyMomentumHUD.py`（三条HUD源代码/资产存在性门禁）独立核查显示链路。

## UE蓝图事实与未完成事项

通过运行中的Monolith MCP 0.23.0（虚幻编辑器资产操作服务）实际修改：`WBP_DBA_UI_PlayerFrameStatus`（头像侧角色状态）中生命y=0、能量y=24、气势y=46，共9节点；`WBP_DBA_UI_PlayerStatus`（技能区状态）中生命y=0、能量y=20、气势y=40，并显示中文“生命”“能量”“气势”，共10节点。主`WBP_DBA_UI_CombatHUD`（战斗HUD）把两处容器分别扩大至60、58高度。新建`WBP_DBA_UI_PlayerFrameEnergyBar`（头像侧能量条）并编译回读5节点；四份相关蓝图在最后成功的编辑器会话中均报告0编译错误、0警告，且`save_packages`（保存资源）返回4/4；新增中文标签又独立编译、保存一次，0错误、0警告。

另外已新建`WBP_DBA_UI_EnergyBar`（技能区独立能量样式）真实.uasset（UE原生资产），但在尝试将技能区现有EnergyBar的蓝图子类替换成该新资产时，Monolith连接发生`WinError 10054`（远程连接重置）；随后9316端口健康检查失败。故**子类最终替换、编辑器重启后重新编译、C++ UHT/Editor/Client/Server真实构建、正式新手村/双端OwnerOnly复制与移动端视觉验收均未在本任务中证明通过**。暂时技能区EnergyBar复用旧MomentumBar外观类，但它是独立控件实例，C++已明确以Energy属性及蓝色样式驱动；这不能冒充新蓝图子类切换验收。独立新资产继续保留供编辑器恢复后原生引用替换。

此期间另一个会话正在运行UE5.8的UnrealBuildTool（虚幻构建工具），本会话不停止、重启或覆盖其编译/资源流程。现阶段证据只允许认定源码、蓝图结构、资源保存及静态门禁部分完成，不得宣称完整客户端联机已通过。

## 2026-10-14 续审执行结果

- 已统一更新平台GAS V3.1、GamePlatformCombat（平台战斗插件）及DBAClient（SSLM客户端）文档，历史UI蓝图验收记录增加2026-10-14覆写说明；保留原有反射类与Widget资产身份，没有重命名既有资产。
- 追加原生事件测试：直接修改ASC（玩法能力系统组件）Energy（能量）为30，验证只更新能量视图、气势仍40；随后将Momentum（气势）设48，验证气势视图更新而能量仍30。该测试源码已写入，UE自动化尚未执行。
- 实际重新执行并通过：`ValidateEnergyMomentumHUD.py`（三资源HUD静态门禁）、`ValidateCompactGASAttributes.py`（2具体属性集/10字段门禁）、`ValidateCombatHUDBlueprints.py`（既有战斗HUD门禁）、`ValidateZodiacAbilityIntegration.py`（生肖技能49项门禁）。`python -m py_compile`（Python脚本语法检查）成功，`git diff --check`（代码差异空白规范检查）无错误；存在原工作树的换行提示，未擅自覆盖其他会话文件。
- 本会话最后重试Monolith端口9316健康检查，curl退出码28（超时）；同步只读检查仍有另一个会话的`UnrealBuildTool.dll DivineBeastsArenaEditor`（编辑器构建任务）运行，其构建过程中需独占引擎DLL。为了不破坏该任务，没有重启编辑器或并发链接这些DLL。因此**本次新增Energy属性C++编译、完整蓝图冷重载与新EnergyBar子类重链接仍为待验**。

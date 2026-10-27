# GlooPrint项目蓝图批处理计划

用户要求使用GlooPrint重新排列项目全部蓝图。本次复用已安装GlooPrint 1.1.0的测量、布局及事务提交代码；默认间距和布局比例保持插件现有设置。该操作只面向编辑器资产，不进行客户端或服务器构建、Cook或部署。

- [x] 在GlooPrint编辑器模块添加薄批处理入口，动态枚举/Game和本工程内容插件的Blueprint及地图内嵌LevelScriptBlueprint。
- [x] 先输出逐资产、逐图清单和不支持原因；复制原始资产及侧车文件到本工程Saved备份目录，保存原始哈希。
- [x] 分资产处理：使用GlooPrint原生算法；比较节点身份、连接、引脚值与非布局节点属性；仅保存发生变化且编译通过的资产。
- [x] 原生保存后独立UE进程重新加载，比较图语义指纹与保存前布局；失败项目单独保留原文件和原因，不通过降低校验伪造成功。
- [x] 保留编辑器，交付准确的资产、图、节点变化及未处理清单，并按用户授权交还资源迁移任务。

特殊情况：空图或单节点无须移动；动画姿态图、状态机图及其他GlooPrint不支持的Schema保留并登记。源码桥接只存在第三方Editor模块内部，不创建第二套排列算法或运行框架。原有未提交内容以本次开始时的真实文件为备份基线，不使用Git重置覆盖。

## 实际结果

已检查591个蓝图资产和60张地图。使用GlooPrint 1.1.0原有测量、布局及事务提交函数，实际修改并原生保存329个蓝图资产、334张图，共调整496个节点；每个被保存的蓝图均通过排列前后编译和图逻辑指纹校验。262个蓝图不需要改动；60张地图均没有内嵌关卡蓝图，因此没有保存或更改地图。

292张空图或单节点图保持原位。另有20张GlooPrint不支持的图：6张AnimationGraph、5张AnimationStateGraph、2张AnimationStateMachine、7张AnimationTransition；未更换其他算法或强行排列。

首次批处理325个资产成功，4个旧作者工具生成的蓝图因节点缺少RF_Transactional被插件保护拦下。随后按UE原生节点创建合同初始化事务标记，保留GlooPrint撤销保护和布局算法；第二轮仅这4个资产产生变化，新增5张图、42个节点，失败为0。最终报告合并两轮真实结果，第二轮未变化资产的文件哈希必须等于首轮结果，避免重复计数或覆盖并发修改。

独立UE5.8进程在NullRHI、只读Python入口中重新加载验证：651个文件哈希通过；329个修改资产内384张图的逻辑及布局指纹通过，0错误。原始651个包的备份也逐一重新计算哈希通过，未根据Git历史覆盖用户现有内容。

备份复核中有3个审核地图的备份路径超过260字符，普通Python路径查询最初返回不可用；改用Windows原生长路径前缀重新读取后，651份备份全部与原始指纹一致。未移动、删除或重建这些备份。

批处理桥接的原生保护测试`GlooPrint.ProjectBatch.LogicGuard`通过，验证坐标及事务标记变化不影响逻辑指纹，连线、引脚值、节点身份、用户注释变化会被检测。新增头文件首次编译时修正了Text头文件路径和UE5.8的Blueprint.GetAllGraphs接口调用。构建仅为GlooPrint Editor模块，未冒充完整Client/Server构建或联机测试。

期间按用户明确授权先完成本任务，与“排查帧率不显示”对话协调暂停共享编辑器写入。原编辑器中7份未保存火法/冰法材质先复制磁盘原件，再经Monolith原生保存现有内容，记录在`PreservedDirtyMaterials/Preservation.json`。材质未参与本次蓝图排列；其后恢复资源迁移任务的编辑器所有权。

## 证据位置

- 原始资产备份：`Game/Saved/Validation/GlooPrintProjectLayout20261027_Apply/Backup`。
- 首轮、重试及最终逐资产/逐图报告：`Game/Saved/Validation/GlooPrintProjectLayout20261027_Apply`、`_Retry`、`_Final/Report.json`。
- 构建、备份验证、独立重载、编辑器交接：`Saved/Validation/GlooPrintProjectLayout20261027/`。其中`ColdAudit.json`为独立进程结果，`BackupVerification.json`为原始备份复核，`EditorHandoff.json`为真实PID/地图/保存状态。
- 本次没有Cook、部署、提交或推送。图表可读性的最终偏好仍由用户在编辑器中审核。

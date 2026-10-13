# 通用材质函数库 P0—P10 执行进度（真实证据）

日期：2026-10-13。固定主工程`DivineBeastsWorkspace/Game/DivineBeastsArena.uproject`（神兽联盟主工程）。本次Cwebcodex会话`wc_sess_pmv6pY2Ody5CFtMf`（执行记录身份）。初始Git：main / 0a0cf10（工作区初始clean）。

## 进度清单

| 阶段 | 已交付 | 状态 | 后续验收门禁 |
|---|---|---|---|
| P0 基线 | 现有7MF+2M+1MPC静态合同；插件读档；真实扫描脚本 | 部分完成 | UE资产注册表/依赖者/Shader实际数据 |
| P1 归集合同 | 201项逐条唯一ID、14类归属、29项核心候选、覆盖与三层规范 | 文档完成，须引擎复核 | 与UE5.8真实可用表达式核对 |
| P2 核心函数 | 已有七MF与两母版的作者工具复查，新增UE审计器准备 | 未完成真实资产验收 | UE材质图、创建保存回读、Shader |
| P3 环境自然 | 28候选已归属 | 实物待验证 | 雪/苔藓/湿润/泥土试验 |
| P4 地形水植建筑 | 54候选已归属 | 实物待验证 | Gaea/PCG/RVT/十类PBR |
| P5 VFX技能 | 34候选已归属 | 实物待验证 | Niagara真Emitter/Renderer/Alpha |
| P6 UI | 18候选已归属 | 实物待验证 | Monolith真实UI材质、主题/移动端 |
| P7 角色贴花后处理 | 34候选已归属 | 实物待验证 | SurfaceMark贴花、角色局部参数/成本 |
| P8 项目演示 | 演示职责及接入边界明确 | 未创建本轮演示 | 平台中性地图与插件开发门户 |
| P9 验证 | 201项索引静态校验脚本和UE只读审计脚本已编写 | 真实运行待报告 | Python静态门禁、UE Editor/Client/Server、Cook、GPU |
| P10 文档归档 | 能力索引、缺项矩阵、迁移及回滚规则 | 文档部分完成 | 人工签收与发布等级 |

## 已知阻断
### 2026-10-13 本轮UE5.8启动请求（非已启动）

已向Cwebcodex提交真实`UnrealEditor.exe`（虚幻编辑器）和正式`Game/DivineBeastsArena.uproject`（神兽联盟主工程）的启动作业`detached_0TgmAmK0jtV4sUQoeomvfrtArpA6eVC4bmZMMXXtrJE`。最后检查为`agent_queued`（尚未执行）；Runner上4个其它会话的长寿命作业占满执行槽位。Monolith服务健康但编辑器未运行；无法进行真实UE资产清查、材质创作或Shader编译。本会话旧验证/诊断两个未启动排队作业已经取消以减少排队，未中断别的会话。所有资产/视觉/双Cook状态继续标记“未验证”。


已通过导出机器台账与中文索引执行独立结构核验，确认201条唯一编号、29项首批核心与7项稳定合同在输出数据内一致；此检查仅对**导出内容**有效。Runner内静态验证作业仍在队列，UE资源没有据此完成。


Cwebcodex同一Runner存在其他会话活跃/排队Jobs（作业）；本轮Monolith（虚幻编辑器连接服务）进程可查询，但执行API明确返回“Unreal Editor not running”（编辑器未运行）。因此本次不能把静态文档和作者脚本当作已生成UE真实材质。本次没有覆盖或删除已有.uasset/.umap资源，也没有抢占其他长寿命Runner工作。

## 文件与运行入口

- `Docs/Implementation/Materials/MaterialCapabilityRegistry_V1.json`（201项能力机器台账）
- `Docs/Implementation/Materials/MaterialFunctionLibraryIndex.md`（完整中文索引）
- `Tests/Architecture/ValidateMaterialCapabilityRegistry.py`（静态一致性测试，实际执行结果以作业报告为准）
- `Tools/Unreal/Surface/InventoryMaterialLibraryUE58.py`（仅UE5.8真实编辑器可运行的Asset Registry只读清查）
- `Docs/Implementation/Materials/MaterialAssetInventory.json`（10项稳定合同，不代表UE验收）
- `Docs/Implementation/Materials/MaterialValidationEvidence.md`（验证门禁及真实阻断）

实施关闭条件是P2—P9完整实证并由P10人工审核，而不是P1“所有候选均已分类”。

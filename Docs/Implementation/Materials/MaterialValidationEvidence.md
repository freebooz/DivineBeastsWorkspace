# 通用材质函数库 P0—P10 验证证据及阻断记录

**日期：** 2026-10-13。以下仅记录本次真实工具反馈与已有源码可直接证实的静态合同，不继承旧验证结论作为新通过。

## 本轮已执行

1. 通过Cwebcodex（代码插件）绑定`DivineBeastsWorkspace`（神兽联盟工作空间）：会话报告主分支`main`，HEAD `0a0cf10bdd898818d11474f4357bee7f96e8843d`，工作树在本轮初始检查为clean（干净），但同一Runner存在其他活跃任务。
2. 只读检索和读取`GamePlatformSurface`（平台表面）、`GamePlatformWeather`（天气）、`GamePlatformVFX`（视觉特效）、`GamePlatformUI`（用户界面）、`GamePlatformPCG`（程序化内容）以及`AGENTS.md`（工程规范）。
3. 静态确认七个既有MF（材质函数）、两个M（材质母版）和八参数MPC（材质参数集合）由`GamePlatformSurfaceAssetContract.cpp`（资源路径合同实现）登记，实际文件与材质图仍需UE检查。
4. 已将201候选能力写入`MaterialCapabilityRegistry_V1.json`（机器台账）及`MaterialFunctionLibraryIndex.md`（中文索引）；201项只是**设计分类入库**，不代表完成201个UE材质函数。
5. Monolith（引擎编辑器连接服务）状态接口报告server healthy（服务进程正常）；实际调用返回“Unreal Editor not running”（虚幻编辑器未运行），因此**本轮没有执行引擎图形资产创建、Shader编译或保存回读**。
6. 已检测同工作区仍有另一个会话的长寿命Runner进程与排队任务；不终止他人进程，不在未知资产状态下强制启动共享Editor。
7. 已通过Cwebcodex导出项目机器台账和中文索引，独立用Python读取导出副本核对：201条记录、201个唯一能力ID、29个P2核心候选、7个稳定MF合同；机器分类数量为MF 98、Other 60、NativeReuse 24、ML 12、ExistingContract 7。Markdown索引中的201个编号无重复。此为**导出文件的结构检查**，不是Runner中的全套静态验证，也不是UE资源验收。
8. 正式Runner发起了`python Tests/Architecture/ValidateMaterialCapabilityRegistry.py`（能力台账验证），但截至本次记录该作业仍为agent_queued（等待执行），没有退出码和测试输出；不能称其通过。


## 2026-10-13 编辑器启动专项执行（用户本轮指令）

- 通过 Cwebcodex（代码插件）明确续接原材质库会话`wc_sess_pmv6pY2Ody5CFtMf`，仍绑定`DivineBeastsWorkspace`（神兽联盟工作空间），`main`分支`0a0cf10`；工作树含本轮材质文档和其他会话的背包/UI修改，未覆盖其变更。
- Monolith（引擎连接服务）`status`显示进程健康，但实际`monolith_status`调用返回`Unreal Editor not running`（虚幻编辑器未运行），不可据此认为已连接编辑器。
- 向Cwebcodex提交锁定`F:\\UnrealEngine-5.8.0-release\\Engine\\Binaries\\Win64\\UnrealEditor.exe`（UE5.8编辑器二进制）启动请求，目标唯一项目`E:\\poject\\feebooz\\DivineBeastsWorkspace\\Game\\DivineBeastsArena.uproject`（神兽联盟主工程）；作业ID：`detached_0TgmAmK0jtV4sUQoeomvfrtArpA6eVC4bmZMMXXtrJE`。
- 执行系统已接收请求，但作业状态`agent_queued`（Runner队列中），`command_started=false`（未执行启动命令），没有PID、编辑器窗口、进程退出码或日志。因此本次**未实际启动虚幻编辑器**。
- 经任务清单核查，同一Runner有4个其它会话的长期运行作业占满执行槽位；为优先处理本次启动，已停止本会话旧的台账验证排队作业`wc_job_I6NysfNZ657osMxr`及先前由本对话创建的排队诊断`wc_job_N8BBHO_8Zq25IE3L`（两者均未开始执行）。没有取消其它会话的长期运行任务。
- 待编辑器真正启动且Monolith证实已连接、主工程身份正确后，才能运行`InventoryMaterialLibraryUE58.py`（UE只读材质盘点）、增量材质作者、Shader编译与验收；当前这些门禁均未通过。

## 尚未取得证据（不得标记通过）

- UE5.8 Asset Registry（虚幻资产注册表）全面扫描：**未执行**。
- 既有七MF、两M、一MPC的实际原生类型、图节点和保存状态：**未执行**。
- 本计划P2—P8新增MF/ML/M/MI及Review地图真实生成：**未执行**。
- Shader（着色器）编译、VFX Niagara编译、UI Monolith创建和样板预览：**未执行**。
- Editor/Client/Server（编辑器/客户端/专用服务器）全目标构建及自动化：**未执行**。
- Client Cook/Stage（客户端烘焙/暂存）、Server IoStore（服务器资源归档）对比：**未执行**。
- ProfileGPU（GPU性能）与移动设备实测：**未执行**。
- 人工视觉验收：**待用户确认**。

## 阻断因素与解除门禁

- 运行任务隔离：在确认当前Editor未被其他工作占用、当前主工程确认为`Game/DivineBeastsArena.uproject`后，才能利用Monolith或Editor Commandlet（编辑器命令行工具）操作二进制资源。
- P0真实资源状态不明：先跑`Tools/Unreal/Surface/InventoryMaterialLibraryUE58.py`（UE材料资产清查）生成UE报告，并将原生类型、引用、Shader信息回填台账；本文件不虚构其输出。
- P1的实现方式是技术决策建议：批量生成MF前需用锁定UE5.8实际节点与消费引用验证。
- UI资产必须用Monolith MCP创建/编辑，若编辑器不运行，不可降级成手写虚假Widget资源。

## 通过状态

文档层归类与静态所有权登记：已生成；P0真实资产验收及P2—P10运行时交付：**受引擎状态限制，尚未通过**。

> 每完成一项真实引擎操作时，应在此追加准确时间、工程路径、资产包名、引擎版本、执行命令、退出码及日志/截图路径。不要将计划里约定的测试步骤提前填成成功记录。

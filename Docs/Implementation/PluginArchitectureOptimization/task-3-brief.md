# Task 3：UI资源租约与异步根布局
# 共同约束
- 用户已授权“给出实施计划最佳方案，然后按计划执行”；本轮在固定项目原工作区增量修改，不提交、不推送、不删除工作区。
- 项目：agent:freebooz-4b15fc0c7cf14418:divinebeastsworkspace-888d2615；实际根目录 E:\poject\feebooz\DivineBeastsWorkspace。通过 Cwebcodex 读取/修改，先读 AGENTS.md 与所属规范。不得把本地 scratch 当成项目。
- 保留现有47个代码插件、17个内容包、三层边界和稳定插件/模块/反射/资源身份；不增设第二工程或 DevHost。
- 开始时有其他 PCG 任务的10个未提交文件；本任务无权覆盖、还原或停用其他任务。只修改本 brief 所属文件，其他问题写报告。
- 源码采用 canonical edit_project_files 结构化编辑；现有文件须带 fresh read_revision。direct 不可用时按 tool_manifest 使用 call_runtime_tool。
- 不修改 Docs/Architecture/解决方案总体目录规划说明_V1.3.0.md 或共享执行台账，由主代理统一登记。
- 不并行启动 UE 构建/Cook/Editor，不关闭已运行编辑器。主代理统一执行昂贵验证；可独立执行有意义的轻量回归。
- 中文职责、线程、取消与生命周期注释；声明代码验证、Automation、Cook/Stage、视觉/联机证据各自边界，不虚报通过。
- 不派生子代理；完成后写对应 task-N-report.md，列文件/接口/关键设计、实际测试命令与输出、待主代理验证、疑虑。聊天仅返回状态与报告路径。

## 独占范围
Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient；该插件内UI接入文档。不要改UI二进制资产、配置、其他模块或平台Data公开API。
## 已确认问题
DivineBeastsAbilityBarViewModel的ProfileHandle通过UAssetManager.LoadPrimaryAsset(UI bundle)直接持有，Reset只CancelHandle未形成项目Data租约；BeginDestroy/Unbind等代次已存在。DivineBeastsAbilityUIProfile直接继承UPrimaryDataAsset，PrimaryAsset身份DivineBeastsAbilityUIProfile:HeroDefinitionId，不能当GamePlatformDefinitionBase传AcquireDefinition。DivineBeastsUIClientSubsystem.EnsureDefaultRootLayout使用DefaultRootLayoutClass.LoadSynchronous，且InstallRootLayoutClass成功后立即SyncPrimaryScreen。
## 目标设计
使用现有IGamePlatformDataService.AcquireResources/ReleaseResources：通过AssetManager只读查询profile软路径，租约异步获取profile，再获取所需图标资源（可批量/去重），代次/弱caller验证，解绑/换英雄/销毁只释放本调用者租约。保留原资源身份和UI表现fallback。不得全局UnloadPrimaryAsset。
根布局改为LocalPlayer所属异步Data租约，显式pending/failed/ready状态与代次。初始化、控制器切换、外部安装布局、Deinitialize都须一致；不产生重复请求/安装后Sync递归重入/失败无限重试。保持原可配置反射软类字段。以已加载资源ResolveObject获得类，不能在回调隐藏LoadSynchronous。
## 验证
审查现有相关Automation并新增有意义生命周期回归，涵盖过期完成、英雄切换释放双方、局部玩家/控制器变化、销毁以及缺失资源fallback；不以单纯源码字符串断言替代行为测试。不得用新helper和helper测试掩盖未接线。报告完整新测试名、实际轻量验证、所需UE构建/Automation，由主代理统一执行。写 task-3-report.md。

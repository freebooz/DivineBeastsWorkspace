# Task 1：统一服务器表现资源排除规则
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
Build/Game/CookFoundation.ps1；新建 Build/Game/ServerPresentationExclusions.json 及必要的同目录规则工具；Game/Config/Custom/{DedicatedServer,VillageServer,VillageDevelopmentServer}/DefaultGame.ini 与 DedicatedServer/DefaultPakFileRules.ini；Tests/Foundation/Scripts/TestFoundationPackagingConfig.ps1 与新增针对服务器排除规则/容器的 Tests/Foundation 或 Tests/Assets 文件。不要修改别的Cook入口，先报告接口需要。
## 已确认问题
CookFoundation仅向Cooker传入-CustomConfig=$CustomConfig，Stage保留DedicatedServer receipt。DedicatedServer、VillageServer、VillageDevelopmentServer配置排除集合不一致，VillageServer缺大量UI/VFX/Surface项；Development缺BirdFlock和Snow。DedicatedServer Pak现整体剔除Common/Heroes（当前均为表现），注释却声称英雄包含权威定义。权威技能定义当前归DBAGameplay。
## 要求
建立明确schema、纯表现目录列表和有限实际profile映射的单源JSON。确定性生成/验证被管控配置段，保留每个profile专有地图、AlwaysCook和开发技能开关；生成工具必须拒绝未知profile/危险泛化路径，不重写无关配置。Cook入口先检查漂移，成功后使用真实stage/pak/utoc清单进行禁入校验（依据现有工具能力，不能拿配置推断Cook通过）。路径边界规范化，避免/Game/Foo误配/Game/Foobar；外置对象也考虑。保留服务器必要碰撞、导航、动画及权威数据，不扩大未证实剔除范围。没有实际资源目录的历史排除项可以兼容标记，不虚构新包。
## 验证
先添加能暴露当前缺漏的有意义回归并执行得到RED（失败证据），实现后执行同一测试GREEN。覆盖三个profile、漂移、规则边界、空/损坏容器证据和允许权威数据。不要为简单文案另写测试。主代理负责UE Cook。报告精确命令、退出码及新增文件，另说明如何调用规则生成和容器验收。

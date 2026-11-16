# SSLM 全游戏操控统一实施计划

需求：以现有新手村为基准，统一 OpenWorld、Village、MainArena 的鼠标和角色操控；用户已要求直接完成，不再问询。

设计：在 DBAWorldsRuntime 提取唯一的 PlayerControls 组件，拥有鼠标捕获、左右键镜头、双键前进、键盘运动、缩放、选择与本地界面意图。World 控制器保留现有准备报告及资格；项目 Arena 控制器继承通用 Arena 控制器，仅转发引擎输入生命周期。Arena 现有就绪事件向共享组件投影本地资格，不改变服务器准入、比赛或 GAS 权威。UI 绑定共同组件，旧 World 控制器接口保留转发兼容。

1. 补真实 Arena 控制器安装与共同输入行为回归，先记录失败；保持现有 Village 控制、选择、门交互测试。
2. 提取共享组件；两个控制器薄适配；竞技服务装配项目控制器；输入客户端语义轴进入同一组件，避免鼠标绕过拖动规则。
3. UI 菜单、名称板和最终镜头事件改为组件来源；换 Pawn、失焦、菜单、旅行、销毁均清理旧状态，不添加业务轮询。
4. 运行原生回归及依赖门禁；完整 Editor/Client/Server 增量构建、必要 Cook/Stage；先核对实际双服务器和客户端版本，再运行验证。
5. 核验两场景的左键观察、右键转向、双键前进、滚轮缩放、UI 不穿透、松键/失焦/转场清理；保留截图和日志，区分自动回归与人工手感验收。

审核重点：竞技控制器不得继承 Experience 资格 Provider；不得让 Village 依赖 Arena；同一输入不得由 Controller、Pawn、语义输入重复执行；其他系统持有的输入锁不得清零；最终镜头变化必须事件通知；火球和小地图资源保持不变。

执行裁决：在用户指定工作空间原位增量修改，保留共享编辑器和已有原生资产，不创建另一工程/工作树，不提交或回退其他任务修改。证据及临时产物统一放在工程 Saved 下。

## 实施与验证记录

实际根因是 MainArena 原先直接使用平台 Arena PlayerController，新手村则使用项目 World PlayerController 中的专用鼠标逻辑；二者没有共同输入消费端。现在原逻辑已移动到唯一的 UDivineBeastsPlayerControlsComponent，旧 World 控制器只保留准备、资格和兼容事件转发，新项目 Arena 控制器只转发原生输入/镜头生命周期。服务器在 InitGame 的现有事件中选择项目 Arena 控制器，早于任何连接创建；没有将 Experience 资格 Provider 带入竞技，也没有修改准入、伤害、追踪或战斗版本校验。

统一规则：默认可见指针；左键拖动只观察镜头；右键拖动同时调整角色水平朝向；双键按住前进；滚轮保持原方向、每格50cm及80—1800cm范围；点击界面不启动世界拖动。菜单、失焦、旅行、换Pawn和失活清理捕获、按住、跳跃、技能和点击候选，不清零其他系统持有的输入锁。名称板和菜单绑定共同组件的事件，不新增业务轮询。

增强输入按平台唯一服务确认的实际接收器、活跃Context映射和有效Touch来源，分别接管Move和Look；不使用最近设备提示判断当前动作来源。缺少Look映射时继续使用原生鼠标，避免部分Profile冻结控制；完整接管时不叠加原生轴，保留重绑定、模拟幅度和一次DeltaTime换算。触摸仍由平台签发和结束，本次没有新增项目Touch播放器或第二份来源状态。竞技PlayerState迟到、统计变化和ClientRestart均触发事件重采样。HUD目录统一为GameAndUI；当前普通HUD挂载不会直接应用该目录输入配置，不能将它独立认定为实机鼠标差异根因。

最终完整Editor f1da2c09-f015-4718-8ce0-fec5405180be、原生 c1c34a74-df3c-492c-9cbe-7f8d7ea4d457 的17项全部通过，覆盖两种控制器、真实服务器装配、移动/镜头、输入重复与部分Profile、真实平台Touch签发、迟到PlayerState通知、菜单、门交互、TAB、名称板及HUD目录。早期RED、编译夹具错误和缺少视口世界的失败均保留，未改写为通过。原生测试不是实际网络或人工手感验收。

依赖Pester共7项、6项通过；其“无环”用例将Game/Saved内历史工作树也当成活动模块，得到272份构建文件但只有92个模块名，重复计数导致失败。独立核验当前92个唯一活动模块和HEAD基线均无环，本次只新增ArenaRuntime/InputClient到DBAWorldsRuntime的依赖；完整Editor也已真实编译链接。原失败与独立核验分别登记，不宣称原脚本全绿。

216份火球资产和6份小地图/UI资产逐一SHA核验未变。编辑器已恢复PID48988，实际Monolith9316；原新手村地图及用户镜头恢复、无脏包、t.MaxFPS=0及r.VSync=0均经原生独立检查。后续双端构建、Cook/Stage、实际部署版本门禁和实机证据统一归Saved/Validation/UnifiedPlayerControls20261102，状态以该目录记录为准。

## 打包与运行交接（2026-11-03）

完整Client/Server增量构建 d6ab3b67-b5c2-4d14-89a4-c4a041a12f95、Client Cook/Stage efb6f1a3-1543-4e27-8410-eb7c4dbbe09c、Server Cook/Stage 487d6f04-403f-4785-8fa5-604bbdfcec97均通过。客户端容器709项必需资源、其中82项音频均存在，无禁用资源；服务器表现剥离门禁通过。UI颜色RHI报告未因本次操控代码变化失效，真实VerifyOnly通过。

部署前发现Docker引擎已停止。恢复时两处旧运行目录中的零字节IPC重解析点阻止启动；未执行工厂重置、删除容器或卷。停机后将仅含socket的目录原位完整保留，重建空运行目录，原有五个验证容器已恢复运行；备份位置和两次诊断失败分别记录在DockerRuntimeRecovery.json、DockerSecretsIPCRecovery.json、DockerJointIPCRecovery.json。控制面重启后旧实例注册不存在，独立查询再次确认不存在，并核对无客户端、无比赛、控制面启动晚于旧服务器及精确进程身份后，才回收旧进程、注册新版；没有将API拒绝伪写为Drain成功。

新版Village PID65096、Boot f95abea2-422c-496b-bafa-e5cb38fb7145，MainArena PID26296、Boot 2a397a74-1c5a-4c88-b778-0b06e5c2af42均来自上述Server Stage。启动前端前的真实版本门禁通过，绑定同一Editor构建、共享战斗/复制输入、双Cook及实际exe哈希/PID/Boot/UDP端口；证据为FrontEndVersionGate.json。

双客户端运行62e97bbc-b103-4f70-a3dd-c356fb9a597e已启动，实际PID13436、27540，均已完成引擎初始化，日志实际记录t.MaxFPS="0"、r.VSync="0"，无致命或复制版本错误。未自动登录。Windows当前锁屏（LogonUI存在，桌面操作返回拒绝访问）；已按computer-use规则请求用户解锁并分别使用player01/player02登录。当前启动成功与版本一致已经验证，新包真实新手村/竞技场准入及两场景鼠标手感尚未签收，不能把原生回归或旧包的竞技场成功冒充本轮实机输入验收。编辑器写入窗口已交回已授权的小地图任务，无需其重复执行已完成工作。

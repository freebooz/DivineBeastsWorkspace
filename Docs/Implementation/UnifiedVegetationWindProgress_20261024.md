# 统一植物风场执行记录

计划：`UnifiedVegetationWindExecutionPlan_20261024.md`。2026-10-24开始，尚未完成。

## 决策与现场

- 执行前HEAD=f0bc6603；保留当前未提交角色/血条、地形及湖岸改动。
- 按用户明确要求直接在当前工程实施；原生编辑器与人工未保存场景均位于此路径，不另建遗漏现场的工作树。不提交或回退其他工作。
- 既有PIE运行中，编辑器PID=71656；正式地图及三份非本任务材质未保存，资产阶段前先保护现场。
- 初始E盘仅约0.7GB，拟使用独占临时输出；2026-10-25实测E盘已余216GB、C盘38GB，可正常生成独占新输出。本任务未清理或搬动旧内容。
- 任务1→4：Baseline组件/材质合同成为接线清单；先读取当前实际场景，不使用旧数量。
- 任务2→3→4：Weather快照→Surface中立端点→MPC/材质；统一世界坐标和连续相位，不增加逐株轮询。
- 任务5→7→8：全场景覆盖→冻结输入构建/Cook→实际服务器与双客户端；旧包和旧PID不作为新验证证据。

## 状态

- 任务1：已保存40个实际Actor、8份ST9材质及正式地图/MPC的只读基线和原包备份。首次Transform repr含内存地址，场景保护比较改用纯数值分量；旧证据保留。
- 任务2/3：d23c2610原生编译实际报缺少WindYawDegrees、WindGustFrequencyHz、SampleWindPhaseCycles，完成RED；随后实现Weather量化/最短弧/相位锚定、Surface端点/原子参数合同及事件投影。
- 完整Editor f5bcc241实际链接成功，但之后源码变化使其过期；c14bcd46及669c202e均因编辑器DLL占用链接失败，不作为通过证据。
- 原PIE已受控结束，重新确认无脏包后退出Editor完成链接；未保存覆盖任何人工工作。
- 任务4/5真实材质接线与保存已执行，任务6—8仍在验收。

## 2026-10-25续接

- 用户曾要求“暂时无法暂停，先完成源码和工具”；该阶段没有制作风uasset、Cook或操作现有两客户端。用户随后再次明确“没有风，请实现”，现已恢复实际资产实施。
- 离线工具13项、服务器剥离策略33项通过；配置预检失败不写源配置、原子替换失败回退、保留明确天气选择、MPC改名/重复/默认值错误均有回归。
- 原生4c3da0fb实际执行8项：7通过、既有调度重入Timer测试1失败。根因为首次Tick只激活Pending且同帧不再推进，夹具已改跨真实帧验证，未忽略失败。
- 独立审查4项Important已实现修正：配置写前全量预检/回退；内容就绪刷新提交前捕获代次；MPC全名称/默认值与函数实际连线校验；既有GameState OnRep时钟通知驱动风端点重投影。原生59d6df8e十项实际全部通过。无额外轮询或新服务。
- UE5.8 SingleFile 8221c727实际编译4份最终C++源文件通过，不链接、不更新DLL、不能用于Cook资格。后续完整Editor结果另记，不能拿指定文件编译冒充整目标。
- 世界预设仍由既有Data定义拥有：新增空默认的`InitialWeatherPresetIdsByWorld`只为正式Village选择已有晴天Definition，显式选择、自动天气及开发积雪序列保持优先。
- ST9原生Motion没有Time/全局输入，复用引擎Motion，仅派生Wind和母材质；不机械复制无改动函数。原始UV与RGB保持，数值权重保护与最终WPO/法线有限值保护在适配支路完成。
- 原型树正式与植物库默认MI均改接共享Core母材质的作者流程已补齐；不直接在World再复制第二份风实现。
- 风四项作为完整组校验，缺风不得半更新，但原八项雨雪表面继续写入并明确返回风合同失配，避免分阶段接入抑制积雪。
- 新手村配置准备已实际apply，审核目录进入明确NeverCook/服务器视觉叶子；4a499da8完整Editor成功。之后新增时钟诊断及有效WPO上限读取入口，需08ad43fb重新取得完整Editor证据。
- 原生创建保存平台两函数、项目ST9派生母材质/风函数，八份ST9 MI接线；正式原型树和植物库默认MI共用Core母材质WPO。现有八天气预设接入统一世界风，其他雨雪温度保留。没有保存覆盖正式地图。
- 已处理非均匀缩放引起ST9风方向旋转：末端向量缩放改成统一平均标量；原生UV权重不改。原型树实际启用Nanite，已设置母材质WPO位移上限40cm，继续核对实际覆盖材质。
- 原生PlatformContract/ST9审计通过。Coverage旧报告只因Nanite补证未完成失败；08ad43fb完整Editor成功后，独立PID67796的ColdReload覆盖MPC、真实函数/材质连线、实际Nanite覆盖材质及位移上限、正式场景与实例保护，全部通过。
- PIE真实风MPC为yaw=0、intensity=0.2509804、gust=0.2、frequency=0.1。ViewportWillow固定镜头48帧已取得，真实GPU像素有变化；独立SceneCapture黑图无效，不作为视觉证据。固定镜头仍受原场景较暗光照限制，人工方向/根部验收待完成。
- 原生时钟测试失败原因已定位于未初始化夹具World的AActor::ProcessEvent早退；限定UObject分发执行真实引擎thunk/虚OnRep，不改变生产权限、全局脚本开关或放宽断言。真实日志server=200、offset=200、notifications=1。
- a8800d68最终完整Editor通过；59d6df8e十项原生Automation通过且前后指纹核对通过。cebf5ab1开始Client/Server完整增量构建，2885编译输入、787合同/资产冻结到本次Inputs.json。
- 独立NeverCook审核地图已由Monolith创建保存：三植物四Yaw StaticMesh/HISM及非均匀缩放。实际GPU方向0/90/180及零风动态帧已取得，审查录像Review_Static_HISM_Directions.mp4；夹具使用0.8强度便于观察，不冒充正式晴天0.25强度或服务器天气切换测试。
- cebf5ab1完整Client/Server构建通过；386782f0客户端Cook/Stage、UI原色RHI、角色/UI/82项音频容器门禁通过，真实ExportBundleData包含平台两风函数、MPC、ST9风函数与母材质。
- a31b2792首次Server Cook真实失败：四份世界原型树MI改接Core母材质后，世界AlwaysCook扫描触碰NeverCook母材质。按实际五份纯材质叶子新增`/DBAWorldPack_Village/PCG/HillsideForest/Materials`服务器排除；模型、碰撞、PCG定义、导航和地图均不剥离，新增回归先红后绿。
- 6c541f86再次失败证明StaticMesh仍持有材质引用，材质叶子剥离方案已撤回，四份受管配置恢复原快照。实际Monolith确认原型树ISM为NoCollision、导航关闭，Core母材质与既有World无风母材质PhysicalMaterial均为空；正式地图、模型、实例与客户端资源均未改动。
- 最终采用三种专服DefaultEngine.ini中的单对象CoreRedirect：Core原型树母材质→既有World无风母材质。仅专服加载，保留模型与World MI，不复制第二份风实现。621c64da完整Editor配置证据通过；b3436c2d真实Server Cook/Stage与容器剥离通过，18项风运行包交付检查零缺失、零Server视觉泄漏。
- 原始2885输入及787合同资产逐项复核：唯一最终差异是三份专服INI的精确追加段；还原追加段后的原字节SHA与Client Cook前完全一致。因此复用386782f0有效Client包，双端实际Stage所有可执行文件/DLL与本次完整目标产物一致。四份未引用植物库默认MI仅原生保存，按引擎依赖裁剪不要求进入本场景包；独立PID4636冷重载记录12项真实场景/模型默认材质，运行材质缺失仍严格失败。
- 最终离线15+34=49项、原生十项通过。已正常关闭旧验证客户端，排空旧本机专服；新Village PID52332、MainArena PID7388来自b3436c2d Stage，端口17782/17881。真实Village完成72个技能CDO加载回归。既有Server CommonUI/InventoryClient扫描告警旧包同样存在，本次未扩大修改。
- 新Client PID39120、2868，实际映像/启动时间匹配386782f0 Stage，两个可见SSLM窗口登录就绪，无已观察到的致命/复制校验错误。保留十测试账号按钮，由用户人工登录；未把登录页启动算作Village/MainArena准入。
- 当前核心统一风与保存/重载/构建/Cook交付已执行。根部0.1cm数值GPU验收、四向/过渡全覆盖人工审查、四档专用降级变体及GT/RT/GPU/P95/P99对照仍未完成，不标记整份计划或Production Ready完成。

## 证据边界

源码测试、原生资产、构建、Cook、准入、GPU测量和人工签收分别记录；未执行项目不勾选完成。

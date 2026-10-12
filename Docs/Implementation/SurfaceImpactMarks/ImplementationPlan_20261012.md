# GamePlatformSurface 命中痕迹系统实施计划（2026-10-12）

## 目标与基线
在现有 `GamePlatformSurface`（平台通用表面插件）中新增客户端局部命中痕迹，不复制第二套插件，也不修改既有雪／苔藓／湿润 MPC 契约。五类为 Bullet（弹孔）、Scorch（焦痕）、Slash（刀砍痕）、Fluid（液体／可替换能量飞溅）、Crack（裂痕）。项目层《神兽联盟》默认用能量／墨迹替代写实血液；绝不将表现结果用作服务器伤害、碰撞或正式可破坏地形判定。

## P0 审查与边界
- 工作区 `DivineBeastsWorkspace` 唯一正式工程 `Game/DivineBeastsArena.uproject`；保留动态 HEAD 与其他开发任务的脏文件，禁止 reset／clean／提交／推送。
- `GamePlatformSurfaceClient` 是 ClientOnly，`GamePlatformSurfaceEditor` 仅 Editor；不得向 Dedicated Server 链接 Material、Decal、Niagara 或项目资源。
- `GamePlatformVFX` 只处理命中瞬态 Niagara；`GamePlatformInteraction` 只产生表面命中事实；`MobaPresentation` 将已确认事实适配到通用服务；`DivineBeasts` 管理技能/风格映射，不把生肖写进平台。
- 蓝图/UE 资产只由真正的 UE5.8 Editor/Commandlet 生成保存；源PNG与脚本都不算完成的 UAsset。

## P1 纯逻辑与插件接口
1. 新增五类通用痕迹类型、命中请求、贴花配置、状态和限制：距离、有效法线、尺寸、淡出、预算、作用域。
2. 一个 `UGamePlatformSurfaceMarkCatalog` 数据资产目录：物理表面类型优先、通配默认回退，软贴花材质路径，五类均有配置；平台不绑定具体世界或技能。
3. `UGamePlatformSurfaceMarkWorldSubsystem` 每客户端世界拥有队列、Data 租约和贴花生命周期；不 Tick，不同步加载，异步资源未就绪时合并/有界缓存，拒绝服务器/错误世界；取消时换代并只释放自己的资源。
4. 使用 `UGameplayStatics::SpawnDecalAttached/AtLocation` 生成组件；按上限与优先级淘汰，只做视觉，不触碰权威碰撞。
5. 提供 `UGamePlatformSurfaceMarkBlueprintLibrary` 蓝图入口及只读诊断，游戏线程严格检验。

## P2 资源与独立演示
1. 为五类准备各自独立的透明遮罩、基础颜色、法线与粗糙度（至少正式可运行实例），材质使用 UE5.8 `Deferred Decal` 域、有效透明通道，绑定真实 PNG／Texture2D。
2. 专用 Editor 工具在插件内容挂载点创建和校验目录、Texture2D、`M_GP_ImpactMark_Master` 与五种材质实例；不得覆盖已存在或其他会话正在编辑的资源，资源生成前做内容指纹与占用检查。
3. 在唯一主工程现有 Plugin Demo 体系增加独立测试入口或演示地图。墙、地、移动网格、斜面和各表面类型可人工选择，记录 Shader 编译/材质/截图，测试冷/热加载、消退/淘汰。
4. 未经真实UE保存/回读的资源一律记为未完成，不能用只读源码或文件名替代视觉验收。

## P3 三层接线与端侧
1. MOBA层只适配已确认命中事实，默认资源丢失时安全降级；项目层使用风格资产决定五类表现并允许关闭液体。
2. 实际战斗裁决、Chaos 断裂、网络同步及可保存破坏仍由现有权威Gameplay/World拥有，贴花不会生成实体破洞。
3. Niagara 粒子通过已有 VFX 通道按命中瞬时触发，持久痕迹留在 Surface，避免重复所有权。
4. 客户端只同步持久且影响其他玩家视觉的一小组授权事件，避免传输整张渲染目标。

## P4 验证与关闭条件
- 静态：插件描述/模块依赖、三层边界、资源目录、公开合同及中文注释、git diff --check。
- 逻辑：五种形态完整覆盖；非法 NaN/Inf、零法线、越界半径/尺寸、无世界/Server、重复事件、预算、物理表面回退、Load 取消/过期完成、世界销毁和多PIE隔离。
- 真实：Editor／Client／Server 目标编译、自动化实际测试导出、真实 UAsset 读取和演示审核；可执行才 Cook/Stage 并审计 Server 材质及模块剥离。
- 完成状态分别标记 `代码`、`UE资产`、`真实自动化`、`Cook`、`人工视觉验收`；任一环节失败保存原始日志，不为凑完成修改报告或跳过用例。

## 交付和回退
- 目录：`Game/Plugins/GamePlatform/Presentation/GamePlatformSurface` 内新增 Marks 业务；新资产尽量独立目录 `Content/Marks`。项目专属主题留在第三层。
- 实施分批：P0/P1 → 编辑器真实资源 P2 → 上层接线 P3 → P4 全目标验证，不强制终止他人 Editor 或 Runner 进程。
- 本轮仅实施有明确验证证据的环节；缺少图形编辑/第三方批准时列为阻塞或待人工，不修改外部仓库资源身份。

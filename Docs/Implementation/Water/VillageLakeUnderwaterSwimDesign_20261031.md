# SSLM 新手村原生湖泊、水下表现与游泳潜水设计

日期：2026-10-31。状态：用户已批准并要求继续实施；明确本轮自由游泳/潜水，不加入氧气或溺水伤害。工作空间 `DivineBeastsWorkspace`，主工程 `Game/DivineBeastsArena.uproject`，UE5.8.0。当前分支 main，HEAD `09ca3e37ed0cc7c891b057d1c0a4b9ad1ecce999`；保留用户已有修改和此前插件场景/字体工作。

## 用户要求与当前事实

用户要求用 WaterBodyLake 替换新手村湖泊，并在此基础上完善水下后处理、光散射、焦散、游泳和潜水。该要求已经授权正式新手村的集成；历史 P1 记录中的“不得迁入正式村庄”不再成为本次集成的阻碍。

当前正式 `L_Village_Start.umap` 的 SHA256 为 `befce449f7783e73be033411d07c94095fb37c780408f0c36ff36cc7f521f1a9`，与 10 月 29 日原生替换记录完全相同。该记录和独立冷重载证据确认 `HillsideWater_Lake` 的实际类已经是 `/Script/Water.WaterBodyLake`，GUID `974AFDFCFB0D3524837CE50E21A0E6A8`；24 个控制点、18,000 个地形采样、河流和两个 PlayerStart 未变。本轮仍须通过 Monolith 再核对真实对象和材质，不能重复创建第二个湖体。

已读取的现有实现：

- `GamePlatformWaterRuntime` 只有 Box 水域查询、浅/深水与可游泳资格的低频权威复制；`bCanSwim` 是水域规则，不会切换 CharacterMovement。
- `DivineBeastsGameplayCharacter` 已挂唯一 WaterInteractor，但移动只用水平控制器朝向，空格只绑定 Jump，没有潜水升降输入。
- `GamePlatformWaterClient` 只监听水域事件并向唯一 Presentation/VFX 链提交涟漪/水花。没有完整水下光学控制链。
- 原生替换脚本曾关闭湖体 Primitive 碰撞；本地 UE5.8 `WaterSubsystem.cpp` 的水下后处理通过水体碰撞查询判断最终相机位置。必须核验并恢复必要的查询碰撞，不能只填一个后处理材质。
- 原生 `CharacterMovementComponent.cpp` 的 `PhysicsVolumeChanged`、`ImmersionDepth` 和 `PhysSwimming` 依赖 `APhysicsVolume::bWaterVolume`；单独设置 `MOVE_Swimming` 不能形成完整游泳。
- 专服开发配置已剥离 WaterBody 渲染 Actor。游泳必须依赖双端保留的 CPU 水域，不能让服务器加载 Water、Niagara 或水面材质。
- 平台已有 Single Layer Water 兼容实例、水下/焦散材质函数和 Texture2D 文件。其真实类型、图连接、有效参数与视觉效果还须原生检查；不以文件名当作功能通过。按文件名检索未找到显式 Swim/Dive 动作，实施时须审查真实 AnimSequence 与当前骨架。

## 实现路线

用户已经指定 WaterBodyLake，工程规则也要求复用现有平台水系统，因此采用一条正式路线：**原生湖面和相机水下管线 + 同源烘焙的 CPU 水域 + 原生 CharacterMovement 网络游泳**。

曾比较的技术选择如下，均不作为第二套活动实现：

| 选择 | 取舍与裁定 |
| --- | --- |
| 复用原生 Water、PhysicsVolume、CharacterMovement | 使用现有引擎预测、碰撞、移动复制和后处理；需要补精确 CPU 体积及查询配置。采用 |
| 自建游泳移动模式、坐标 RPC 和全局后处理播放器 | 增加第二套运动与表现状态机，扩大网络和清理风险；现有原生能力足够，不采用 |
| 第三方流体模拟/换引擎分支 | 不能解决角色权威和准入问题，且超出现有安装和分层边界；不采用 |

用户已明确选择自由游泳/潜水，本轮不加入氧气消耗、氧气HUD或溺水伤害。

## 1. 湖体与几何

保留实际 WaterBodyLake 的对象身份、GUID、24 点湖岸、湖面标高、河流、地形、建筑、道路、PCG 排除及人工出生点。继续 `affects_landscape=false`，不雕刻、不改变湖底地形、不改导航几何。

在正式地图与现有 `/Game/Development/Water/L_GP_WaterReview` 中建立可独立核验的同一合同：

- 湖面继续使用官方 Water Lake 兼容材质链；先核验当前稳定的 `MI_DBA_Village_Lake`，正式参数与绑定收敛到稳定资源名。现有 Review 资源仅保留追溯，不留下两个竞争的当前绑定。不靠文本重命名迁移资产。
- 水下检测使用原生 Lake 碰撞组件的 QueryOnly 设置及引擎 Water trace 通道；不增加阻挡角色的水面碰撞，不开启地形雕刻或导航影响。具体响应表依据本机 UE5.8 Water 设置读取后确定，并通过真实相机查询证明生效。
- 由编辑器从同一 Lake 闭合样条生成、保存 CPU 物理水域体积，边界覆盖到湖底有效深度；湖岸和高地必须逐点抽样核对，不能用大矩形把陆地包成游泳区。采样精度和几何分解由实施者依据实际轮廓选择，并记录误差。
- 扩展既有 `AGamePlatformWaterRegion`，使其可引用当前世界的原生 `APhysicsVolume`；新湖体的精确包含查询和 CharacterMovement 都使用这组已烘焙物理体积。既有 Box 仅作为未迁移区域的兼容路径，不与新体积同时裁决同一湖区。
- 体积的水面、`bWaterVolume`、摩擦、优先级来自唯一 Region 配置/作者输入。生成器校验拒绝互相重叠且规则或优先级歧义的水域，避免原生 PhysicsVolume 选区与 Region 查询选区分歧。
- 服务端只需 Engine 物理体积和 WaterRuntime 区域；无 WaterBody/材质硬引用。保存时保留引擎真实 Brush/Model 等子对象。几何为空、失效、重复 ID 或指向其他世界均拒绝。

脚部进入水域表示涉水；胶囊中心进入水物理体积才由引擎进入游泳。头部在水下与最终相机在水下是独立状态，不混为一个布尔值。

## 2. 游泳与潜水

保持当前唯一 ACharacter/CharacterMovement，无新坐标 RPC、平行运动器或客户端授权：

- 角色具备游泳资格，使用原生 `MaxSwimSpeed`、`Buoyancy`、水阻力及 Capsule 碰撞。水面漂浮、潜下、升起、岸边退出均由同一原生运动模拟及服务器验证；禁止每帧强制 SetActorLocation/贴水面。
- 游泳时 W/S 沿控制器视线方向运动，A/D、Q/E 保持现有侧移语义；空格上浮，Ctrl 下潜。地面移动和空格跳跃沿用原行为，输入框、失控、死亡、失去控制器时不接受游泳输入。
- 使用普通加速度输入和已有 CharacterMovement saved moves/复制。若真实联机回归暴露原生能力缺口，再在同一组件扩展，不先造自定义移动模式。
- 初始速度、浮力和阻力为可调候选，由实施者按角色胶囊和真实湖深选定并记录，不声称是最终平衡值。必须验证静止浮水不会抖动、下潜可持续、出水不会沿用向下输入。
- 继续由 WaterInteractor 发布只读状态。按实际需要补头部浸没/游泳过渡字段，只有离散变化复制；运动本身不等待 0.3 秒状态采样。UI 先取快照后监听事件，不增加 Widget 定时轮询。
- 动画层只读真实移动模式、速度、头部浸没和俯仰；补水面待机、游动与潜水姿态的原生动画图状态。先查当前骨架/已持有动作；可复用或原生制作共享基础动作，由现有内容所有者持有。不得拿走路循环冒充游泳，特殊骨架未适配须明确登记。
- 死亡、重生、换化身、传送、退出世界和断线时清理输入/订阅；由新 Pawn 和新世界重取状态，不恢复旧水域句柄。

## 3. 水下后处理、散射与焦散

复用原生 WaterSubsystem 对最终相机的水面相交与水下后处理计算，避免按按键或角色脚部推测相机位置，也不加每个 Widget 的 Tick。

- 通用水下材质/函数归 `GamePlatformSurface`，具体新手村材质实例、颜色、能见距离归 `DBAWorldPack_Village`。已有 Water 母材质和平台通用函数先原生读取，增加参数须保持其他地图原默认行为。
- 水面散射/吸收沿用 Single Layer Water 参数；水下随实际视线水中距离计算吸收与入射散射，控制太阳方向性、能见距离和深度衰减。水面上方保持原色，水线附近连续过渡，避免整屏固定蓝色滤镜或频繁闪变。
- 焦散复用已导入的真实纹理及现有材质函数，通过世界位置、水面高度、水下接收深度、表面法线与距离遮罩限制在湖底和浸水物体；平滑动画由渲染材质时间驱动。不得照亮岸上建筑、天空或无关世界，也不为焦散另建 Niagara System。
- 首选在当前水面/水下材质链内完成可见散射与焦散。禁止全局无限范围灯光函数覆盖整个新手村；需要局部补光时仍须明确水域边界、受光面与画质开关。
- Low 保留基本水下识别和必要能见度，关闭装饰焦散/额外方向散射；Medium 减少装饰复杂度，High/Epic 提高局部装饰。具体数值来自真实图和测量，不预先声称 GPU 达标。
- 水下声音可订阅原生相机进出水通知，接入已有 SFX 混音机制；无可用资源时如实登记，不影响游泳。没有请求制作新法术，不扩展当前技能 VFX 审核范围。

官方参考：[Single Layer Water](https://dev.epicgames.com/documentation/en-us/unreal-engine/single-layer-water-shading-model-in-unreal-engine)。实际实现以本机锁定 UE5.8 源码及 Monolith 反射为准。

## 归属与预计改动范围

| 路径/所有者 | 职责 |
| --- | --- |
| `GamePlatformWaterRuntime` 的 Region/Types/Interactor 与 Private/Tests | 精确 CPU 水域引用、只读状态及生命周期；只依赖 Engine/现有共享模块 |
| `GamePlatformWaterEditor` 与 `Tools/Unreal/World` | 同源物理水域生成和校验，湖体原生配置、备份、保存及独立重载 |
| `GamePlatformSurface` Water 材质/函数 | 通用水下散射、焦散、水域遮罩，不硬编码项目资源 |
| `GamePlatformWaterClient` | 原生水下通知和既有表现服务接线，不复制 WaterSubsystem 或 VFX 执行器 |
| `DivineBeastsAbilitiesRuntime` 与当前输入配置/资源 | 项目角色游泳资格、升降输入、现有控制/死亡门禁 |
| `GamePlatformAnimationClient` 与现有角色内容 | 只读动画快照和真实原生动画状态，不改变服务器位移 |
| `DBAWorldPack_Village` 与现有水体审核图 | 正式湖泊绑定、同源物理体积、世界具体视觉实例和观察点 |
| `PluginDemoManifest` 现有 Water 页与案例 | 水上/水下、进出水、泳潜操作入口；沿用唯一 Catalog，不另建测试中心 |
| `Build/Game` 现有构建/Cook/启动流程 | 沿用目标与目录约束，必要时只作精确配置增量 |

不更改账号、角色持久数据、三角色划分、技能权威或竞技规则。不新增插件、内容包、骨架体系或第三方采购。

## 验收顺序

1. Monolith 核对实际项目、UE 版本、湖体身份/材质/碰撞、CPU 区域、相机和当前角色骨架；记录修改前指纹，目标脏包不覆盖。
2. 先建立必要失败回归：岸外/凹岸/高地/水面与湖底、重叠区域、缺体积拒绝、进入/退出/传送清理、游泳输入限制。随后增量实现并观察通过。
3. 在现有水体审核关卡完成真实水上/水线/水下镜头和泳潜闭环，再绑定正式新手村。原生资产创建、Shader/蓝图编译、保存、独立新进程重载分别留证。
4. 对新手村复核湖岸误差、地形高度、河流、两个出生点及道路/建筑/PCG 排除；不要覆盖用户已改植被、AI 或出生点。
5. 完整 Editor/Client/Server 增量构建；保留 UE5.8、Win64/Development、`-NoSharedPCH -NoUBA -MaxParallelActions 8`，Editor 用 `-UsePrecompiled -AdditionalPlugins DBAServer`。只 Cook/Stage 实际受影响内容，所有临时、DDC、输出及证据在当前工程内。
6. 专服容器检查保留 CPU 物理体积而剥离纯 Water/VFX/材质资源。运行前核对实际 Client 与 Village、MainArena 服务器的战斗/复制合同、包、PID；必要时同步构建、Cook/Stage及受控重启，不清账号数据。
7. 原生自动化、双端泳潜运行和人工视觉分别报告。至少包含陆地→涉水→漂浮→前进/侧移→下潜/上浮→出岸、镜头独立过水线、静止、重生/传送/取消/退出、弱网运动修正；验证同一位置另一客户端可见正确状态。
8. 固定镜头原生截图与动态录像显示水上/水线/水下、散射、焦散及泳潜；真实查询 `t.MaxFPS=0`、VSync 关闭，不改玩家持久化设置。四档 Shader/GPU 检查依据实测记录，不把启动成功当人工签收。

## 留给实施者的决定

原生物理体积的 Brush 分解、样条采样密度、可调速度/浮力初值、实际材质参数名、现有骨架动作重用与最小原生动作制作方式，由实施者依据原生读取和测试决定并记录。是否加入氧气/溺水仅由用户偏好决定。实施不得因单个资源缺失伪造通过，须先完成其他已授权部分并列出具体缺口。

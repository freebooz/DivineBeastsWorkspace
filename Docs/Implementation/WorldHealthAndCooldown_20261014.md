# SSLM血条、冷却遮罩与事件通知（2026-10-14）

本次按用户要求新增其他玩家和怪物的头顶生命条，V键及HUD按钮切换显示；技能图标保持可见，半透明遮罩从12点方向顺时针消退。鼠标沿用既有左键环绕、右键转向和滚轮距离控制，新增最终镜头变化通知用于世界UI重投影。法术不消耗气势，仍接受原生GAS冷却和权威资格检查。

## 实装范围与所有权

- 平台GamePlatformUI提供只读世界UI资源请求、名称板数值投影、视口通知和作用域Root就绪事件。原30Hz投影Ticker已移除；平台不知道项目角色和怪物。
- DBAClient的WorldHealthBarPresenter按LocalPlayer／World持有观察，初始化盘点一次已有角色，此后消费Spawn、EndPlay、Destroyed、组件位置、ASC生命及上限、怪物复制LifeState事件。自身已有主HUD，头顶条仅显示其他实体；未知上限隐藏，不伪装满血。最大128项，显示距离6000厘米，头顶偏移24厘米。
- DBAWorlds控制器在引擎完成镜头更新后比较实际最终位置、旋转和FOV，变化才通知；不能以鼠标输入猜测最终镜头。V键只在当前世界交互且未被UI阻断时生效。
- 平台槽位的事件入口设置每实例MID的截止时刻和总时长，渲染材质使用Slate实时时钟播放；Widget不Tick读业务状态。清除动画不放宽技能资格、不补气势。
- Root初始化先于UIManager安装服务时，Presenter在Root就绪后重发已有缓存；Root替换将同一请求迁入新层，强引用与代次检查防止同步重入复活旧Widget。换账号、换世界、Possess、EndPlay及销毁成对解绑。

## 真实资产

### 玩家名称板修复（2026-10-24）

- 用户要求其他玩家角色显示头顶血条和玩家名称，当前LocalPlayer自己的Pawn不显示这两项；自身主HUD沿用原链路。复用既有`WBP_DBA_WorldHealthBar`的`NameText`和`ResourceFill`，本次不修改视觉资产。
- 服务器从可信PlayerData响应中读取**当前已选角色的`characterName`**，通过引擎既有`APlayerState.PlayerName`公开复制；不复制账号、完整Roster或持久CharacterId。旧响应缺少名称时只使用通用显示，不改变原有英雄准入资格。
- 平台PlayerState将姓名RepNotify转为原生通知；项目Character将PlayerState迟到/换绑转为原生通知；项目Presenter先绑定再读快照，换State、EndPlay、Clear时成对解绑，旧来源通知不能覆盖新名称。生命仍只消费GAS真实复制值，不新增Tick或轮询。
- 保留既有128项预算和60米显示距离；后来玩家优先替换最远未选中的怪物名称板，避免200怪物先到时耗尽玩家显示位置。发布时重新核对当前LocalPlayer的Pawn，成为自己后立即撤销已有头顶名称板。
- 原生回归同时覆盖真实UMG姓名投影、姓名晚到/变化/换绑、自己排除、128怪物预算、现有Possess/EndPlay/Root和选中生命周期。构建、回归与后续客户端验证证据归`Saved/Validation/PlayerNameplates20261024`；未完成真实联机和人工画面检查前不宣称运行验收通过。
- 本轮完整Editor `ca272013-bcaa-4f96-833b-d3ed9f0b0c2f`及Client/Server `7dd0bfce-56cd-4312-a274-f0ac7bebbcb0`均退出0；六项界面回归加独立精确筛选的一项服务器名称回归全部Success。原联合筛选计数失败报告原样保留，完整七项集合和运行边界见`FinalValidation.json`。随后Client Cook `322fd754-8f96-446b-9d8e-11c936e02824`的UAT及629项必需资源检查成功，但发布被火球审查特效GPU着色器未声明`Index`拦截；Server Cook `e937e6ef-e20d-4766-8aa7-b98aaf6e4dd6`通过。运行进程未替换，联机未验收。用户后续又调整84份魔法弹速资产，旧Cook结果不能作为新速度版本的准入证据，必须重新完成匹配发布。

均使用正式Game/DivineBeastsArena.uproject、UE5.8和Monolith0.23.0创建或修改并编译保存，未改地图、PlayerStart或源图RGB。

| 资产 | 实际变化 |
| --- | --- |
| /DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilitySlot | 新增CooldownSweep Image，旧CooldownOverlay收起，原图标与边框保留。 |
| /DBAUIPack_Core/UI/Materials/Combat/M_DBA_CooldownSweep | 复用平台RadialCooldown，旋转UV确定12点起始，50%半透明颜色，不复制另一套平台机制。 |
| /DBAUIPack_Core/UI/World/WBP_DBA_WorldHealthBar | 160×44固定名称板，NameText、ResourceFill和ValueText；投影时首次预排版并按DPI居中。 |
| /DBAUIPack_Core/UI/Combat/WBP_DBA_UI_CombatHUD | 添加血条按钮、V键说明、工具提示、18号字及明确导航边界；Monolith可访问性检查0问题。 |
| /DBAUIPack_Core/UI/Root/WBP_DBA_UI_RootLayout | WorldHealthBarClass指向真实名称板类，提供Cook硬引用及运行装配。 |

新编辑器39384冷重载确认上述类引用、名称板和技能槽控件树，关闭前dirty_packages=0；证据为Saved/Validation/Rebuild20261014/d2d4518f-080b-47ca-8c39-c2d608646485/WorldUIAssets.ColdReload.json。不使用已知Gamma错误的Monolith widget截图路径。

## 检查与验收边界

原生回归验证资源变化、未知上限、静止首投影、隐藏后重显、冷却事件参数与清除、Possess／EndPlay／解绑、真实服务Root就绪及替换。鼠标控制用例验证现有输入行为。完整结果、构建、真实RHI颜色、Cook和最终运行身份集中记录在同目录FinalValidation.json，失败历史报告保留；不得仅凭UE进程退出0宣称全部测试成功。

初次UI回归的资源尺寸失败来自夹具未持有TakeWidget返回的Slate表面，已按真实Overlay所有权修正；后续30项回归的生命周期失败来自夹具未初始化World控制器列表，已按引擎真实初始化流程补齐，并增加控制器解析和Possess前置断言。重要源码问题的独立复审包括Root首次安装、同步重入、SameWorld Possess与EndPlay，不以隔离测试代替联机、实际流送和人工画面验收。

## 全局策略与历史遗留登记

用户要求已写入C:/Users/Administrator/.codex/AGENTS.md的“用户界面事件通知全局策略（2026-10-14）”，并同步工作空间AGENTS.md。首次绑定允许读取完整快照，后续状态更新使用通知；连续视觉交给引擎动画／材质，生命周期清理与代次检查必须成对实现。

本轮检索覆盖DBAClient/DivineBeastsUIClient、DBAArena/DivineBeastsArenaClient、GamePlatformUI源码，发现下列存量项，未将本次事件驱动修复宣称为全部旧UI迁移：

| 现有路径 | 现状与后续处理 |
| --- | --- |
| DBAArena/Private/UI/HUD/DivineBeastsArenaHUD.cpp | PhaseClock每0.2秒读取阶段剩余时间，需迁移为阶段事件设置截止值和引擎展示。 |
| DBAClient/Private/Social/DivineBeastsPartyScreenBinding.cpp | DeadlineTimer每0.25秒更新请求倒计时，需随组队观察合同事件接入时迁移。 |
| DBAClient/Private/ViewModels/System/DivineBeastsSystemSettingsViewModel.cpp | PreviewTimeout常驻0.1秒Ticker，需改为设置服务的预览结束／撤销通知，核对显示倒计时。 |
| DBAClient/Private/Development/DivineBeastsPluginDemoLauncher.cpp | 仅开发启动器每0.5秒检测外部进程，需与生产业务轮询区分并改为退出通知。 |
| GamePlatformUI/Private/Feedback/GamePlatformFloatingTextWidget.cpp | NativeTick视觉漂浮动画需核对是否仅引擎动画职责，不能据Tick字样断言业务轮询。 |

Notification／Feedback的一次性寿命定时器与UIManager的下一帧输入恢复属于事件调度，检索命中不自动等同状态轮询。未全量审计Blueprint绑定、其他插件或所有历史UI。后续新建、修改链路必须执行全局策略，不继续新增轮询。

中文说明检查覆盖新增／修改的请求、槽位、投影服务、Presenter、Root、控制器、材质作者工具和真实回归夹具；第三方与生成源码未手改。用户人工登录、联机伤害、实际鼠标画面及视觉验收仍由实际运行确认。

## 用户实测反馈后的鼠标输入修正

用户明确反馈进入新手村后按住左键或右键拖动均不转动。早期MouseAndMovement仅为纯控制策略回归，不能证明真实视口输入；此前的鼠标可用判断撤回。

已依据锁定UE5.8的SceneViewport.cpp:692–705定位：当前游戏视口尚未捕获且模式为NoCapture时，bTemporaryCapture和bProcessInputPrimary均为false，首次鼠标按下不会调用InputKey；控制器依赖该按下才能开启拖动，形成无法启动的环。平台GameAndUI配置、项目CommonUI交接和控制器光标恢复三处均曾强制NoCapture。

三处同步改为CaptureDuringMouseDown，与原生FInputModeGameAndUI一致；UIOnly继续NoCapture，UI先处理与严格Slate命中门禁保持。左键环绕、右键转向、双键组合、松开恢复、真实失焦和进入菜单的Flush清理不放宽；不新增轮询或修改权威资格。

真实FSceneViewport回归位于既有GamePlatformUIInputPolicyTests.cpp，Editor夹具设置非零640×480输入尺寸与PIE游戏分发标记，首次无捕获，实际调用引擎OnMouseButtonDown并计数InputKey；不应用FReply到桌面、不冒充账号或联网测试。RenderCore仅作为Editor测试依赖，发行客户端与专服不新增此依赖。红测MouseInput.RED-587a263b中GameplayMousePress左右键实际均0而期望1，PolicyModes也失败；DefaultActionsAsset成功。随后三模块修复构建e764e9b6-b3f0-4f99-86e1-80d97f623f3f通过，MouseInput.GREEN-34610a15的34项全部Success、0错误，包含真实按下分发及WorldHandoff。1项旧Possession夹具有2条警告，未隐藏为零警告。

本修正证明引擎按下分发和配置交接，不把夹具扩张成已完成用户实际拖动、Controller轴与最终画面人工验收。新三目标构建、Cook及替换进程以FinalValidation.json的最终本轮身份为准。

独立复审另发现暂停后失焦仍无条件写回捕获/锁。MenuFocusPreservesCapture绑定真实WorldContext、控制器和UGameViewportClient，调用公开Suspend/Flush入口；MenuCapture.RED-Retry-13358754实际1项Fail、4错误。初次MenuCapture.RED-4ec732f7在Editor DLL初始化阶段超时，测试未执行，保留失败过程，不计红测证据。捕获与锁写入现仅在未暂停时执行，保持CommonUI菜单所有权。修复模块3c05e7ec-7ebf-41ce-bd1d-3824c75be1a5通过，最终MouseInput.FinalGREEN-e0fe51c4报告35项全部Success、0错误；1项旧Possession仍有2条警告。最终Client/Server构建221e576e-c6d1-49c9-ae3c-a17846ca70e7真实退出0。复审已确认重要源码问题关闭，剩余人工画面边界不变。

最终完整Editor构建99e216ac-ad01-4a65-88e8-3443af5d11ef成功且Eligible；Client/Server Cook分别为4ea54e30-1f5a-4549-b31e-e6bb7dab7f94和d8e2a8b2-4d96-4a94-b143-0ae7af5b3ee1，Cook/Stage与实际容器门禁均成功。最终专服51524、双客户端36540/6208路径及启动时刻与新包匹配，两个SSLM登录窗口响应且无Fatal。人工登录后才能验证新手村实际鼠标画面、联机施法、血条显示及遮罩视觉；本轮未自动认证。当前结果集中于FinalValidation.json及RuntimeObserved.json。

## 2026-10-24 玩家名称与自身隐藏

其他玩家使用既有世界名称板显示真实生命快照及服务器验证的角色名称。服务器从已选角色资料读取characterName，经既有PlayerState.PlayerName复制；客户端订阅玩家状态替换和名称复制通知，首次读取快照，此后事件更新，退出与换代成对解绑。自身当前Pawn不注册世界名称板，个人HUD保持既有行为；不通过Tick扫描玩家或血量。

世界UI继续采用128项预算；玩家进入时可淘汰最远未选中的开发怪物，避免200只怪物占满预算后看不到其他玩家。原生回归覆盖角色名称、自身排除、迟到PlayerState、名称变更、换Pawn、生命周期清理和玩家预算优先级。打包与实际双客户端显示证据归`Saved/Validation/VFXPlayerRelease20261024/`，原生夹具结果与真实联网画面分别报告。

# SSLM法术输入与初始气势修复

用户于2026-10-14要求修复不能施放法术，并明确初始气势为50。本次在主分支增量处理英雄数据、输入命令与验证，业务网关沿用唯一实例28181。

**当前规则更新：用户随后明确“法术不应消耗气势”。本文件早期10/8/12费用与50→40→32→20的运行记录保留为历史；当前法术费用全部为零，初始气势仍为50，最新验证见文末。**

## 实现边界

- 初始50归十二生肖真实HeroDefinition（英雄定义）的Momentum.InitialMomentum，新定义默认值同步。服务器既有角色初始化组件将其写入唯一GAS属性集；法术不应用气势成本效果，也不通过界面或每次施法补充资源。现有气势上限保持不变。
- 平台GamePlatformUI提供中立槽位调用事件；项目DBAClient技能栏只将已授权槽位转换为平台ASC输入。键盘与鼠标最终由GAS及服务器校验，技能图标不持有伤害权威，也不新增请求协议或竞技依赖。
- 控件构造时绑定命令，销毁时撤销本页订阅；视图模型核验本地受控角色、当前授权与输入代次，资源、冷却和禁用状态继续由事件刷新。
- 真实资产操作通过Monolith执行。制作工具为`Tools/Unreal/Characters/SetHeroInitialMomentum.py`，先核对全部身份与上限，再精确保存；调用前备份实际英雄包。地图及人工PlayerStart不在保存范围内。

## 验证目标与当前进度

- 当前真实出生用例不调用测试补充气势函数，三个法术的期望为50→50→50→50；零气势仍可施法，但冷却、取消、迟到来源与螺旋路径门禁仍保留。
- 鼠标回归使用已保存公共技能槽，验证有效左键进入命令、待确认/禁用/空槽拒绝，并检查事件次数；不伪造Widget资产。
- 已完成正常RED：`Native-4ef69a81-9375-4dd6-a7b8-cbe90c75a2b0`实际11项，原有9项成功，新增出生及鼠标2项失败；具体断言是初始0不等于50，以及有效技能槽左键未受理。早期完整名称筛选未命中与Widget夹具Engine Outer错误均未计通过，最终RED已修正这些运行前置。
- Monolith0.23.0已核对正式工程及全部12份英雄定义，原值均0、上限均100；随后精确保存初始50，上限保持100。编辑器关闭前dirty_packages=0。原包备份为`Saved/Backups/SpellCast20261014/cc6fb4df/HeroDefinitions`；该步骤只证明保存，新进程重读记录见下项。
- 已通过Monolith回读真实技能槽：父类GamePlatformSlotWidget、9个控件、CDO为SelfHitTestInvisible且背景Border可命中，现有Widget树能够接收冒泡输入。本次点击修复采用中立C++事件与项目ViewModel命令，未改控件树、布局或视觉纹理。
- 新进程重读已完成：`SpellCast20261014/Reload-e1f90958`（编辑器PID19312）经Monolith只读载入全部12份定义，逐份核实初始50、上限100、稳定身份不变；dirty_packages=0后退出，未保存地图。
- 完整Editor构建`ead408dc-e1bc-440c-b94f-66f12c207328`通过（退出0、77份项目DLL）。随后`Native-a3e62c14-0733-43a1-a136-826ed91c9309`实际45项、44成功：出生50及槽位点击已满足断言，但新输入夹具缺少ULocalPlayer导致输入令牌资格失败。已补齐引擎SetPlayer绑定及错误码断言，仍待新完整Editor构建和重新回归；未放宽运行权限。
- Client预编译尝试`38d8b67c-eaa3-4e9c-90a5-8f576629b3c4`因Sockets预编译清单缺失失败，不计构建成功；当前改用同一锁定引擎的既有源码构建。编辑器重试`6e849824-8558-49b2-b454-9d30c473dca8`因该构建占用Foundation锁未执行，不计通过。
- 新完整Editor构建`9bc93910-65f4-44b3-a52f-8eb7dc04b26f`通过；`Native-2963a96a-c8d3-4ba0-8715-76f600d62969`仍为44/45，唯一失败为新增ULocalPlayer夹具错误Outer触发引擎ensure，业务期望断言失败数为0。已依据引擎Within=Engine规则修正GEngine容器，不屏蔽ensure、不改成功统计；仍待再次完整构建和全45项通过。
- 四并行源码构建`6a33d62a-f778-4ec6-97a6-751280eefeda`在核实PID42648、路径及启动时间后仅终止本轮构建树，退出-1，产物保留。主机32线程、64GB内存、约31GB可用，编译进程实测约1GB/个；据此改8并行增量构建`4a6d00cf-f14c-45fb-805a-fabf6a4be786`，不停止业务后端或旧专服。
- `4a6d00cf-f14c-45fb-805a-fabf6a4be786`实际完成550个动作，唯一失败是MSVC编译`Module.ModelingComponents.3.cpp`时访问异常（-1073741819），没有隐藏错误或禁用模块。四并行增量重试`a84612d5-1cb8-4b99-ab01-4c9a51e74d34`只需4动作，Client目标已退出0；随后Server需874动作，仅精确停止本轮编译PID11268，改独立八并行`ee4fcd83-8a35-4f9b-bfd9-67e7fad8ab01`。该组合Run整体退出-1，不能作为Client/Server双目标成功证明。
- 发布辅助门禁已实际执行：RHI颜色`1c7d1e35-63d1-4e32-8d06-a37c0d811047`通过；继承边界扫描549份Public头、1198类型、205继承边通过；英文命名检查149682项、65插件、0违规。上述均不替代施法回归、Cook或联网验收。
- 完整Editor、Client、Server构建及新鲜Cook完成后，再替换专服与双客户端。业务后端的身份/角色仍为内存开发存储，本次部署保持其容器持续运行。
- 以上为按时间保留的阶段记录，失败或未执行没有改写为成功；当前结果以下节“最终验证与发布”为准。

## 最终验证与发布（2026-10-14）

- 最新完整Editor构建`fd6a121e-a2bb-471f-a306-d2833f69eaef`退出0、证据Eligible，2688份源码输入及77份项目DLL已关联。最终回归`Native-748de867-8b43-40dd-83dd-90c917e59114`实际45项全部成功、失败0；新出生与指针用例各为0错误/0警告。真实出生及GAS输入证明50→40→32→20，重复定义通知保持20，不调用测试补满气势。
- Client目标在`a84612d5-1cb8-4b99-ab01-4c9a51e74d34`的Client结果为退出0，独立Server构建`ee4fcd83-8a35-4f9b-bfd9-67e7fad8ab01`整体退出0。构建均为锁定UE5.8 Development/Win64；Client/Server走真实源码构建，Editor使用既有引擎预编译产物并真实编译项目模块。
- 新客户端Cook`fddba8b9-0ca8-41b0-ae74-be5db89231aa`通过，包括524项必需资源、RHI颜色、鸟群及P7内容门禁。专服Cook`6fa94980-b709-4497-86dd-4ed9d7ae2cc1`通过；3个实际容器、3029条目录、1202条资产项、禁止表现条目0，必需Village地图存在。容器目录检查不等同完整载荷块审计。
- 并行Cook刷新4份受管FileOpenOrder生成日志，`Native-684d9fe8-9986-46db-9ff1-a9407f0fc22a`在启动UE前因当前源码指纹不匹配被拒绝，不计测试通过。完成Cook后先从HEAD校验可恢复字节与完整构建快照完全一致，再将新生成日志备份至`Saved/Backups/SpellCast20261014/CookOrder-4f31be07`并机械恢复，仅处理这4份本轮生成文件；Game/Build无剩余改动，随后独立运行最终45项成功回归。
- 旧专服PID12272经路径/启动时间核验后请求控制面排空再停止。首两次新进程因本次启动命令漏了Profile根目录、公开端点而分别失败；保留`Server-Attempt1.json`、`Server-Attempt2.json`及独立日志，不改成Ready。补齐既有`DBA_SERVER_PROFILE_ROOT`与`GAME_SERVER_PUBLIC_ENDPOINT`契约后，实际运行PID19392，日志`Deployment-7749b95d/VillageServer3.log`；新Boot经控制面校验，Ready世界分配、17781 UDP和DevelopmentMonsterReady均通过。开发分配探测预留1个槽位，未创建账号或改角色档案。
- 两个新包客户端PID39680、45388，证据`Saved/Validation/FrontEndClients/d91dfc14-a767-4bee-ab0d-29fecf953ddd/Clients.json`；均明确使用28181、开发技能及怪物配置，AutoLogin=false。两个SSLM窗口实际存在，两份日志均载入真实WBP_DBA_UI_Login并聚焦AccountInput。只读桌面截图显示Windows锁屏，因此停止界面自动操作；尚未完成解锁后的人工登录、进入Village、双端施法/伤害/VFX及视觉验收。
- 五个`sslm-validation-8238653a`业务容器持续运行，本轮未重建或重启，不迁移/删除内存账号与角色。此处只交付开发验证环境，不能把内存存储视为生产持久化，也不宣称人工联机验收已经通过。

## 人工验证与说明审查

### 独立源码复核的补充事件修复

独立只读复核发现：技能显示缓存由授权、Spec、气势及资源事件推动，但没有订阅平台PlayerState生命周期与非平台GameplayEligibility资格变化。若这些资料先到、Active后到，缓存的禁用状态可能持续阻断鼠标命令。已核对真实角色激活Gate的两条来源，问题成立，不通过移除禁用校验或放宽服务器权限规避。

新增`DivineBeasts.UI.Combat.AbilityActivationEvents.RefreshAndUnbind`使用真实GI/World/Pawn/ASC及两种真实通知源，仅以旧的只读显示缓存作为观察标记；不伪造角色Ready、授权或施法成功。修复前完整Editor构建`b1535d4c-2458-475d-ab80-4a971c8039f6`通过，`Native-a203cda3-cc80-4d85-9eab-395e02bb3958`正常RED，实际1项失败，4个断言准确报告缺失刷新。补丁按当前Pawn绑定两种事件，已有来源不重复绑定、来源变化先撤旧再绑新，换角色/解绑/销毁均撤销；通知只使视图重新核验，不直接授权。增加GamePlatformGameplay的显式Private构建依赖。GREEN及客户端再次发布结果待实际补齐；上节45项和已启动客户端属于补丁前基线，不能作为此补丁已验收的证据。

复核还指出制作工具注释错误承诺失败时提供已执行清单；已将注释改为实际语义：逐包保存非事务，调用前必须整组备份，失败按备份恢复。未再次改写已经验证为50的资产。

### 补充修复最终结果（当前交付）

- 完整Editor构建`36af2cee-1f54-40f6-8853-5bc83f64b3da`退出0、Eligible；最终原生报告`Native-3f658250-fdb2-4e94-a783-e94c0a7ee92a`实际46项、全部成功、失败0，包含新资格事件刷新/解绑用例及原45项。新增用例只验证真实通知进入显示刷新与撤订阅，不将其扩张为真实网络Active顺序验收。
- 独立复核再次读取具体订阅、换源与解绑实现，确认原P1源码问题解决，无新增Critical/Important；制作工具说明Minor关闭。源码审查通过不代替运行回归，本次结合上述46项成功证据解除提交阻塞。
- 最终Client构建`a1f08015-66cb-473f-b7bf-144f34933c46`退出0；最终Client Cook`c2dd4a4d-5263-4d87-b83f-fbfb870c4218`退出0，524项必需资源无缺失及全部既有发布门禁成功。Cook后仅备份/机械恢复本轮两份Client顺序日志，与最新完整Editor快照指纹一致；备份`Saved/Backups/SpellCast20261014/CookOrder-FinalClient-e9abf3f2`，不混入代码提交。
- 精确匹配并停止本轮旧客户端39680/45388后，启动最终客户端37588/40688；证据`Saved/Validation/FrontEndClients/30d8483d-3508-47c3-b0b9-7c288186d926/Clients.json`，明确28181、AutoLogin=false，两份日志均加载WBP_DBA_UI_Login且无Fatal。未再尝试锁屏后的界面自动操作。
- 专服19392、已验证Server包及五个业务服务保持不变，未因客户端补丁重启后端或重新创建角色。三层继承/Public边界已再次检查通过。当前交付范围是代码、真实数据资产、回归、重新发布及双客户端启动；人工认证、双端联机施法/命中/VFX和画面验收仍待用户解锁桌面后完成，未记为通过。

重新发布后使用同一28181网关人工登录，选择已有角色或创建新角色，进入Village。初始气势应显示50/100；开发子鼠样例按2（火焰弹）、3（寒冰箭）、4（奥术飞弹）或点击对应技能图标施法，三个法术均不消耗气势，冷却仍生效。1对应普通攻击，被动图标没有主动施法命令。其他英雄以其真实授权和按键提示为准；开发伤害与射程仍不代表正式平衡数据。

本轮中文说明审查覆盖平台槽位接口、项目面板/视图模型命令、初始气势定义、属性初始化边界、真实资源制作工具和新增回归夹具；已说明所有者、游戏线程、失活解绑、失败结果、代次与服务器权威。第三方/生成源码未手改，项目历史代码的全量中文注释合规审计不在本轮完成范围。


## 2026-10-14法术零气势费用修正

用户明确“法术不应消耗气势”，覆盖早期费用方案。真实36个开发法术曾分别声明10、8、12费用，新的Monolith门禁在修改前实际失败并保留SpellAssets.RED.json。项目Magic数值行现在拒绝非零MomentumCost；开发魔法执行器通过原生GetCostGameplayEffect=nullptr实现无成本，不跳过Commit、资格、冷却、权威伤害或取消生命周期，不引入Mana或施法后补气势。其他非Magic技能仍能声明合法费用，初始50／上限100保持原英雄定义。

- 通过正式UE5.8、Monolith0.23.0窄迁移85个已存在包：数值表1份、GA36份、旧成本GE36份、UIProfile12份。旧GE稳定身份保留但资源修改清空，GA不再引用成本，说明明确“不消耗气势”。整组原包先备份，记录为Saved/Validation/Rebuild20261014/SpellCosts-20261014T130943/Report.json。未修改地图、PlayerStart、图标、Widget布局或VFX。
- 新Editor进程39504冷重载36个法术／12套授权及Profile检查通过，dirty_packages=0，随后正常退出；证据SpellAssets.ColdReload.GREEN.json和SpellEditor.ColdReload.log。颜色发布门禁重新核验通过。
- Editor构建fc3ece2f-94dd-4b20-9f1e-6b5ecf7e254f、Client／Server构建31b1d7a1-7b91-49f4-8186-e8075a7fc86e均真实退出0。Editor复用锁定引擎产物；Client／Server采用完整源码目标构建，未关闭模块。首轮Editor回归编译因const UClass*不能转TSubclassOf失败，已据实际C2664修正夹具类型后重编。此前9b059660预编译Client检查缺少引擎precompiled清单失败，本次源码构建已正确通过，不把失败日志删除。
- 本次官方Automation报告实际25项全成功、失败0：包括Magic数值拒绝旧成本、真实出生50→50→50→50、零气势完整三弹、真实冷却拒绝二次施法、取消、来源代次与同步后继、UI输入和Profile。测试没有把出生资源补满；重复Hero通知另用7点资源验证幂等不重置。证据Rebuild20261014/d2d4518f-080b-47ca-8c39-c2d608646485/SpellNative-827f8a4d。

中文说明检查覆盖数值表合同、原生法术成本入口、迁移工具、资产审核和施法回归；保留源资源身份、开发启用边界及旧历史证据。本段构建／隔离GAS测试不替代真实账号联网施法与人工视觉验收，运行包与进程证据将在Cook完成后补记。

## 本轮最终重新发布（含鼠标输入修正）

此前历史费用、进程和未验证记录保留；当前法术费用为0，初始气势仍50／100。完整Editor构建99e216ac-ad01-4a65-88e8-3443af5d11ef与Client/Server源码构建221e576e-c6d1-49c9-ae3c-a17846ca70e7均退出0。MouseInput.FinalGREEN-e0fe51c4实际35项全部Success、0错误，1项旧Possession有2条警告；覆盖零费用及冷却、资源事件、Root就绪/替换、真实SceneViewport鼠标按下分发、CommonUI交接与菜单Suspend/Flush保护。

用户实测鼠标拖动不转后，已定位并修正世界三处NoCapture造成首次按下不分发的配置。世界采用CaptureDuringMouseDown，UIOnly保持原捕获/锁所有权；没有放宽UI命中、输入锁或Gameplay资格。源码及红/绿回归细节见WorldHealthAndCooldown_20261014.md，旧纯MouseAndMovement不能证明实际画面可用。

真实RHI颜色报告afcce7aa-44e4-4d64-8528-5fd2ad1b7a68通过，最终包沿用同一未变化资产指纹并通过发布复核。Client Cook/Stage 4ea54e30-1f5a-4549-b31e-e6bb7dab7f94、Server Cook/Stage d8e2a8b2-4d96-4a94-b143-0ae7af5b3ee1实际退出0；客户端524项必需依赖无缺失，血条、冷却材质、技能槽及Root均有真实IoStore ExportBundleData。专服表现剥离门禁通过，不把目录检查扩张为完整载荷块审计。

排空并精确停止上一包后，最终专服PID51524，Village注册/怪物就绪/控制面分配及17781 UDP身份通过；最终客户端PID36540、6208，真实路径和启动时刻均匹配新包，两个SSLM开发窗口响应，实际加载Login并聚焦AccountInput，无Fatal。五个开发后端的ready均200；保持人工登录，没有新建测试账号、自动认证或完成真实双端施法/画面验收。

累计清理110.90 GiB生成产物与89份过期临时脚本，正式Build/Tests/Tools、当前鼠标修复包、源资产和报告保留。本轮两份临时部署/启动脚本在消费后删除并记录SHA256；Cook改写的四份顺序日志按已验证备份恢复至完整Editor输入快照，不覆盖其他代码。自有Zen保活助手在两端Cook完成后停止，共享缓存数据未删除。

集中证据：Saved/Validation/Rebuild20261014/d2d4518f-080b-47ca-8c39-c2d608646485/FinalValidation.json、RuntimeObserved.json及Cleanup系列报告。全局UI事件策略已持久化，新增/修改链路事件驱动；存量倒计时/预览轮询登记后续迁移，未宣称全项目旧UI已完成事件化。

# SSLM插件演示中心源码实施与验证边界

日期：2026-10-13。执行依据为已批准方案A、核心T00—T14计划及R01—R06全插件接入计划。此文记录实际源码，不是完整交付或人工验收报告。

## 当前结论与授权

用户要求全部执行完成后再集中人工核验。当前仍未完成全计划，不安排逐批人工核验，也不生成审核人或Passed。Native在正式工作空间main原地增量实施，不创建工作树，不构建、不Live Coding、不Cook/Stage、不提交/推送；没有启动或关闭UE/其他任务进程。

开始HEAD为`eb1fe0e932967ce97078df12c8080cf5121c99b4`，共享main随后由外部推进至`6bed78c42b80ccce07cda1cbdae515c318051f26`、`ec6a8c67afcb2f36a603905bd165e9e7a87ecaf6`及本次续行起点`88d8f1093720a2e01ed32e71ab0637c5778b892a`。这些不是本代理的提交；编目git_head仍表示发现时点，不能冒充DLL版本。材质、背包、Backend、Agones等并行修改不在本任务责任内。

## 三层边界与唯一所有权

- GamePlatform提供中立UI层栈、Data服务及Editor审核机制，不引用项目资源。开发审核源码保留在GamePlatformDeveloperTools，普通Client/Server不依赖它。
- 项目开发装配只增量维护DBAClient的DivineBeastsDemoClient，宿主/目标及构建配置保持开发客户端/编辑器限制；没有新增第48个代码插件、资产管理器、VFX执行器或业务主流程。
- 项目公共演示UI规划归DBAUIPack_Core的`Development/PluginDemos`，不是第四架构层。正式默认地图不变，演示模式不启动正式登录/会话。
- Catalog按GameInstance作用域持中央Data定义租约；WorldSubsystem按World持案例令牌、适配实例、资源租约和一次性计时器；ViewModel仅在活动页面订阅事件，Widget只显示值及提交命令，没有业务Tick/HTTP。

## 逐任务状态

| 任务 | 已写入的实际实现 | 尚未完成或验证 |
| --- | --- | --- |
| T00/T01 | 65插件身份、37地图文件候选、Schema2只读门禁、历史源及中文派生文档 | 全插件功能范围仍未审计；历史提交/哈希自动持续门禁仍待强化 |
| T02 | 平台DefinitionBase派生目录、稳定值合同、Private完整结构校验、Definition测试 | UHT/原生编译、真实DataAsset、加载验证 |
| T03 | Data加载/租约、完整快照原子发布、真实插件启用状态、功能/案例查询、显式歧义路由 | 实际资产加载、禁用/缺资源及多实例运行 |
| T04 | 完整RunToken、GI注册句柄、私有状态机、准备/清理截止仲裁、同步重入及跨World取消 | 原生生命周期回归、真实退出/跨地图/多PIE |
| T05 | 事件驱动HubViewModel、平台HubScreen、虚拟化菜单、类型化参数及命名控件合同 | 实际Widget/焦点/导航、同步清空/替换VM场景原生复现；报告历史统计未接入，当前不虚报 |
| T06 | BirdFlock公开能力适配、局部RestoreBirdEdits及失败保留原值 | 真实Scene组件/GPU环境正常演示、失败恢复与其他Batch互不影响实跑 |
| T07 | 真实ResourceBar Widget操作/恢复、真实Data EngineCube共享租约/取消、Samples测试源码 | 资源条Monolith WBP、多实例共享/取消原生运行 |
| T08 | 脱敏原始状态/日志/索引、用例完整冻结、平台证据门禁、Editor待审桥；新增受控来源观察及显式不可变机器证明提升工具 | 当前raw仍binary_verified=false；真实提升未运行，完整当前版本导入/截图附加/报告历史统计与签收UI未闭合 |
| T09 | Catalog作者脚本、Monolith UI制作规范、只读导出合同门禁 | 新资产创建/编译/保存/独立重载、真实颜色/设备适配；规格拆分详情/控制/报告独立资产未交付，不能用合并Hub当全规格完成 |
| T10 | 平台UIManager根布局/页面定义接线、完整资源租约/回调身份、旧路径兼容 | 真实新旧资产引用迁移、关卡重载、去程/返程验证 |
| T11 | 默认不执行、VerifyOnly/Execute分支、完整Editor门禁、必需测试/前后指纹；原生报告实际文件读取及字段类型回归 | 原生全部测试未执行；完整提升成功/进程级故障端到端仍未验证 |
| T12 | 新资产/完整Editor启动门禁、受控Launch记录/内容端点、开发地图白名单、默认/Server排除；Stage配置/参数检查源码 | Client独立产物证明未实现，启动明确拒绝；旧AlwaysCook目录仍需收口，真实Cook/Stage产物未验证 |
| T13 | 三种样板已形成独立功能、案例、用例工作单 | 三类样板真实运行及证据完整交付，集中人工核验尚未开始 |
| R01—R06/T14 | 保留逐插件分期与稳定身份；R04新增CoreIdentity真实值API回环及拒绝案例 | Core仍partial，其余功能/插件逐项接线、18内容包画廊、三端/网络/发行证据及最终收口 |

上述“已写入”仅说明源码存在，不是任务完成、性能通过或部署可用。

## 当前编目样板与首项Core接入

| 功能身份 | 独立案例 | 真实能力与限制 |
| --- | --- | --- |
| DBAClient.BirdFlock.Count | Count、Reset | 既有World/Scene组件数量操作；0..512及默认32是开发夹具输入，不是项目批准的CPU/GPU预算 |
| GamePlatformUI.ResourceBar | Normal、InvalidMaximum | 指定真实平台ResourceBar控件；50/100与非法最大值是UI夹具，不是玩家权威生命值 |
| GamePlatformData.Resources | SharedLease、Cancel | 唯一中央Data服务实际EngineCube资源；共享释放及取消不创建第二资产管理器 |
| GamePlatformCore.Identity | Normal、InvalidName、InvalidVersion | 实际TryCreate/ToString/TryParse/相等/哈希回环；非法名称和版本零保留拒绝，不生成生产资源/账号身份 |

当前4项功能、9案例、9用例；每个案例有独立参数、前置条件、步骤、预期及必需证据。四插件范围均partial，65插件完整分母仍未知。`wired`仅表示源代码接线，不能读成`launchable`、RuntimeVerified或HumanReviewed。VFX/Presentation不自动获得签收。Core的其余能力没有借此宣称已审完，详见[Core工作单](CoreIdentityWorkOrder_20261013.md)。

## 核心调用与生命周期

1. JSON作者源经只读验证后，由真实UE作者脚本派生目录定义；SourceFingerprint是原始JSON字节SHA256，不是编译指纹。发现地图单独作为诊断，不按路径猜所有者。
2. Catalog取得中央Data定义租约，校验完整候选和实际发现插件集合后一次发布。失败保留旧有效快照、释放本次租约；注册工厂按GI句柄撤销。
3. 世界预检按ScenarioId查询，`FindScenarioDefinition(Id, OutScenario)`返回bool与值副本，不公开内部指针。地图比较使用MapAsset包身份，同地图多案例仍是独立身份。
4. Prepare/操作/Stop只在当前RunId、WorldId、Generation及操作序号下完成一次。准备和清理截止使用单调时间，在回调入口再次仲裁，不依赖计时器恰好先投递。切图须先完成自己的清理。
5. 新RootLayout及ScreenDefinition由平台UIManager装配；拒绝覆盖别的Root或已有页面注册。页面失活、VM清空/替换、同VM重开后，旧同步广播不能再刷新或发送命令。
6. Bird复位只恢复本次捕获的字段，失败保留记录供重试，不自动停止其他Batch。适配析构仅在旧World仍有效且未退出时最佳努力恢复，不改写清理失败。
7. 终态在发布事件前冻结本次原始证据。超过4096条诊断事件保留显式截断标记；该上限只是内存保护，截断日志不得冒充完整事件轨迹。

## 证据与人工边界

运行证据仅追加到`Game/Saved/GamePlatformValidation/<RunId>/Review/`，拒绝链接路径及覆盖，保留真实状态/错误码但省略外部动态错误正文。用例冻结功能目的、前置条件、步骤、预期和证据类型；旧运行不能借新用例同版本字段取得通过。

平台导入验证实际state_dump的运行/案例/功能身份、终态、完成操作数量和清理状态，并重新读取文件摘要。`runtime_completed`不由布尔声明自行成立。二进制验证另需独立证明及完整原生测试；当前写入器明确`build_version/content_fingerprint=unverified`及`binary_verified=false`。Editor桥只创建/绑定NotRun待审报告，不接收审核人或Passed参数。

本地摘要/JSON并非签名服务，不能用于证明恶意外部材料可信。真正人工作出结论时仍由唯一平台显式确认入口写追加决策；本批没有调用该入口。人工集中核验前必须先完成自动运行、证据版本绑定及剩余实施。

新增机器来源、版本提升、完整内容范围、单体目标拒绝及输出合同见[证据绑定说明](PluginDemoEvidenceBinding.md)。实际Writer不自签；工具只追加新机器待审索引。项目桥的当前内容仍unverified，完整签收路径不能据此标记完成。

## 真实编辑器阻塞

锁定引擎实际位于`F:/UnrealEngine-5.8.0-release`。本批Monolith状态两次返回“Unreal Editor not running”，只读进程检查没有UnrealEditor。对既有完整Editor结果执行：

```powershell
pwsh -NoProfile -File Build/Game/TestPluginDemo.ps1 -VerifyOnly -EngineRoot F:/UnrealEngine-5.8.0-release -BuildResultPath Saved/Validation/FoundationM0/20261012-0400-4000-8000-000000000011/Build/result.json
```

实际退出1：当前源码输入与完整Editor构建不一致，需要重新完成完整Editor目标。该命令只校验证据，没有启动UE、执行新构建或原生测试。禁止构建约束仍生效，不能偷偷重编译或启动旧DLL去制作依赖新类型的资产。

资产恢复的前置条件是后续统一构建提供当前源码匹配的完整Editor证据，再连接正确的`Game/DivineBeastsArena.uproject`与Monolith，读取版本/动作目录。之后才可制作资产、编译/保存/重载并将实际结果追加到内容包MonolithGenerationManifest；本批不写虚假Saved资产条目，也不使用错误Gamma截图路径。

## 检查与剩余风险

最新真实离线命令、数量及退出码集中记录在ExecutionProgress.md。Python/PowerShell合同回归、头引用/继承/命名检查不替代UHT、链接、UE自动化或Cook。Samples测试要求真实Monolith ResourceBar资产，缺资产会失败，不以Skip/零测试算通过。

中文说明内容复核范围包括本批Demo公开合同、Private核心流程、三类样板及CoreIdentity、UI/Editor桥、平台审核增强、来源观察/提升工具、测试/作者/启动和配置说明。注释写明端侧、作用域、失败/取消、租约及权威边界；历史全项目尚无全量内容审计，不宣称整体合规。

独立源码审查发现并已写修正：不存在的转换头、数值前缀误解析、FName长文本、关联用例归属、旧合同未冻结、raw完成状态未验证、Bird复位所有权、超时回调竞争、同步换VM/清空VM。空VM崩溃的原生蓝图复现仍待完整Editor及真实Widget测试，不能把静态重读当运行回归通过。

既有全局设计门禁仍因`Game/Saved/BirdFlockAuthorHost/BirdFlockAuthorHost.uproject`计入正式工程总数2而失败；未删除宿主或降低门禁。新源码仍可能存在未被静态检查发现的UHT/API/链接或运行问题。回退需逐文件评审、保留旧资产和并行改动，不重置整个main或批量改稳定身份。

本批合法Editor私有依赖曾触发旧PluginCompositionAudit无条件正则的Client误报。新增回归真实红→绿后，审计器仅求值可确定的单Target.Type枚举条件；未知条件/else保守保留，插值/原始字符串/预处理整源不剪枝，普通注释/字符串花括号不参与结构判断。新增合法插值嵌套字符串的漏检回归亦真实红→绿并独立复核，Editor仍需真实插件声明、Client真实泄漏仍失败，不以模块白名单隐藏问题。全套Pester15通过，最终设计基线剩上述1项旧工程数量失败。全工作区diff检查另报并行Agones后端测试末尾空行；本任务路径检查退出0，未改动该文件。

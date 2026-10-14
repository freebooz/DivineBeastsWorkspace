# SSLM新手村组队与竞技场实施记录

计划：`Docs/superpowers/plans/2026-10-14-village-party-arena.md`。规格：`Docs/superpowers/specs/2026-10-14-village-party-arena-design.md`。执行起点main／88f994dd，用户要求直接Native执行、不问询、不构建、不创建工作树。

## 裁定与执行边界

- Ruling: 用户“请执行，不要问询”覆盖技能的规格／计划再次确认门禁；按已给出的建议细则推进，不扩大到生产发布、凭据配置或不可逆操作。代价：建议时间参数尚无实测，后续可能调参。
- Ruling: “不构建”优先于技能要求立即运行编译型TDD；先写回归，再实现，UE与Go测试留待统一构建，未实际运行不得计为红绿通过或完成验收。代价：语义、链接和运行缺陷仍可能存在，当前不得合并／上线。
- Ruling: 用户已明确允许当前main原位修改，不创建技能默认工作树；保留Agones实施记录及其他任务变化。代价：未验证源码留在当前工作区，需要按清单隔离交付，不把其他任务变化纳入提交。
- Ruling: 用户追加“设计组队等用户界面图并创建对应蓝图”后优先整理UI设计与制作输入；正式编辑器缺少依赖DLL退出且禁止构建，不通过禁用必需父类、替代工程或伪造资产绕过Monolith。代价：本轮只能交付设计与规范，真实蓝图0个，整体任务不算完成。
- Pre-flight: 任务1产生普通队伍规则，任务2以事务实现跨队唯一索引；领域AddMember不能单独证明全局唯一。
- Pre-flight: 任务3沿用任务2主体幂等／Revision；任务5消费契约快照，不从本地状态推断命令成功。
- Pre-flight: 任务4处理真实权威事件，任务5只投影；Monolith必须等实际父类可加载，不以源码存在作为蓝图父类证据。

## 当前进度

| 工作包 | 源码状态 | 验证状态 |
| --- | --- | --- |
| 1 普通队伍领域 | 邀请／申请／终态／转让／离队／解散／准备失效与序号边界已写入；不是完整限时准备会话 | 回归源码已写，未编译／执行 |
| 2 事务与公开接口 | Redis单队条件事务、成员唯一索引、回执、迁移门禁、3公网及3内部HTTP和对应gRPC接线已写入；Shared契约已更新 | gofmt解析与格式检查、Proto重新生成比对、YAML引用解析已执行；真实Redis、HTTP／gRPC及完整统一生成未执行 |
| 3 队列与准入 | 仅匹配幂等主体／内容摘要与条件锁定路径增量；票据、名单锁和玩家预留仍非统一事务，消费者／逐人确认／恢复未完成 | 未构建／未执行 |
| 4 单局歼灭 | 本轮未修改UE死亡／复活／胜负／重连路径 | 未执行 |
| 5 客户端交互 | 本轮未完成Social命令桥、观察订阅与阶段适配；制作规范明确PendingNativeCommandAdapter | 未执行 |
| 6 真实UI／地图 | 3幅原创PNG、公共5页＋竞技4页蓝图制作规范；真实编辑器启动失败，新增蓝图0个，未改地图 | PNG哈希、9页控件唯一名／焦点／父类源码归属静态核对；无UE编译保存重载／RHI／运行证据 |
| 7 集成复核 | 一次独立只读源码审查已完成；发现双请求终态Revision问题，修补并补回归源码 | 不构建／不执行测试，当前整体不具备完整合并或上线条件 |

本表只记录实际发生的工作；文档与测试源码不替代编译、联机、Cook或人工核验。

工作区并发记录：执行期间HEAD由`88f994dd`经外部操作推进到`4cd4d3b76603a318cc81d389d12fd0804d17e928`（提交消息`2`），其中收录本轮多数文件及其他任务文件。本轮代理未执行git commit／add／reset／push，不将该外部提交宣称为本轮审核通过或本轮提交；不回退或拆改它。后续静态差异改用执行前`88f994dd`基线核对，避免只检查工作区剩余文档而漏掉源码。Agones记录在11:03前的核对与起点SHA-256`128EEA19078B8661B38F01AF8D8CDEED9E479C02958F0403613615C316826E77`一致；11:05再次观察到它由其他操作继续变更，当前SHA-256为`95E26F318720E0D5AA4F8D2647B43C9EC86B44A5CE952AEB73BFEF19DC5D8C7E`。本轮不读取其新业务内容、不编辑或回退它，不将该文件变更纳入本功能审查。

## 源码与迁移边界

已修改／新增的主要范围：

- `Backend/internal/modules/party`：邀请60秒、申请120秒、上限、处理权限与不可反转终态；名单／队长／活动变化使旧准备失效；加入序号只由服务端分配，不用map或字典序决定队长。
- `Backend/internal/app/{matchapi,gateway}`：Actor取认证会话、命令绑定主体和内容、条件Revision、本人授权快照、邀请收件箱及安全错误；不是把完整聚合直接公开。
- `Backend/internal/platform/cache/redisstore`及两个生产组合根：单Redis的WATCH／MULTI成员唯一索引与回执，显式拒绝Cluster；生产启动检查迁移schema，缺失时失败关闭，不静默回退空索引或内存仓储。
- `Backend/internal/transport/{httpadapter,grpcadapter,grpcclient}`及内部Proto绑定：沿既有可信内部网络扩展3项组队交互；**未新增服务间认证机制**，内部端点必须继续隔离于公网。
- `Shared/Contracts/GamePlatform/OpenAPI/{party-interaction,party,gateway}.openapi.yaml`：普通队伍公网及内部真源与兼容说明；未手改注册表生成物。

运维迁移尚未执行：停止所有旧组队写入后，才可在维护窗口以`PARTY_MEMBERSHIP_INDEX_MODE=offline-migrate`执行有界检查和索引重建，完成后移除维护开关，常规启动只检查schema。重复玩家出现在多个队伍、无限TTL、超限或锁丢失均不发布成功标记，必须人工核查真实记录，不任意选队。单次迁移写入可因错误停在中途，但无schema标记时新业务保持关闭；本轮没有连接或修改真实Redis。

匹配请求查询键由旧裸ClientRequestID改为主体作用域摘要。**成员索引迁移不迁移旧匹配票据**；上线前需排空旧票据／相关名单锁或补完整兼容迁移，不能让15分钟内旧票据与新键并存而被误判为无请求。票据保存与名单锁目前仍分次写入，完整任务3未实现，禁止宣称并发报名／取消或崩溃恢复已安全。

OpenAPI统一注册表入口为`Backend/internal/tools/contractcodegen`。当前未找到已编译生成器，按“不构建”未运行`go run`，因此Backend／Shared生成注册表新鲜度仍有已知缺口；不人工改生成源码。内部Proto使用已安装且匹配锁定版本的独立生成器执行，不编译Go或UE。

## 界面资产与编辑器实证

本轮交付说明：[组队与竞技界面设计及蓝图制作说明](PartyUIDesignAndBlueprintDelivery_20261014.md)。

3幅概念PNG已真实落盘，分属公共UI内容包、可选竞技插件和本实施设计目录；原始提示词、修正记录、路径及SHA-256在`Design/ImageGenerationPrompts_20261014.json`。两份插件内制作规范共9页，所有创建／编译／保存／重载状态为false，不写入Monolith生成成功清单；AI原图未导入UE，不是完整运行纹理。

本轮确实启动了锁定`F:/UnrealEngine-5.8.0-release`的正式`Game/DivineBeastsArena.uproject`，带`-NoCompile -NoCompileEditor -NoHotReload -NoLiveCoding -unattended`；没有触发构建。编辑器因缺少GamePlatformInteraction／DBAWorldsRuntime和DivineBeasts客户端导入失败退出。实际日志为`Game/Saved/Logs/VillagePartyArenaEditor.log`，行1787、2129—2130、2472—2473、3163分别记录依赖缺失与DBAArena模块加载失败。后续Monolith状态仍为`Unreal Editor not running`；当前不存在可操作的正式编辑器UI资产连接。

## 已实际执行的检查

| 检查／命令 | 实际结果 | 不能推出的结论 |
| --- | --- | --- |
| `gofmt -w`后对本轮24个Go变更文件执行`gofmt -l` | 退出0，输出无未格式化文件；仅Go语法解析与格式处理 | Go类型检查、编译、测试、竞态或生产可用性 |
| `protoc --version`、Go及gRPC生成插件`--version` | 30.2／1.36.8／1.5.1，与现行锁一致 | 所有生成器或运行时依赖均已验证 |
| 锁定protoc对match-service.proto再次生成并比较两个Go绑定SHA-256 | 退出0，两个文件逐字节一致；复核目录`Game/Saved/Validation/VillagePartyArena/fcb8b5cd-4746-4716-974b-7a1fdca8a4ae/Codegen` | 已编译／链接或RPC互通 |
| Python严格YAML解析及本地`$ref`定位 | 退出0，3份规格、131处引用解析成功 | OpenAPI正式生成、注册表新鲜度或线协议兼容验收 |
| PowerShell解析2份蓝图制作JSON | 退出0，9个唯一页面、树内控件名唯一、焦点名存在，原生父类声明源文件存在且模块归属匹配 | 父类可被当前编辑器加载、Widget资产存在或命令已绑定 |
| 3幅PNG读取尺寸与SHA-256比对 | 退出0，1536×1024、1672×941、1672×941；哈希与原稿清单一致 | 4K实图、UE纹理导入、RHI颜色或人工视觉验收 |
| `Tests/Architecture/TestEnglishProjectNames.ps1` | 退出0，103365项、65份插件描述、命名违规0；报告`Game/Saved/Validation/VillagePartyArena/fcb8b5cd-4746-4716-974b-7a1fdca8a4ae/ProjectNames.json` | 插件职责、架构、资产引用或运行功能正确 |
| `git diff --check 88f994dd`（排除无关Agones记录）、总体目录前缀比较、说明本地链接核对 | 退出0；总体目录仅追加、保留8个既有冲突标记；本轮基线内.uasset／.umap变更0，交付说明图片和文档链接存在 | 既有冲突已解决、真实蓝图已创建或其他任务已验收 |

以下检查未执行：Go test／vet／race、真实Redis及迁移／并发／TTL故障、HTTP／gRPC互通、统一契约生成／检查、UE三目标构建、原生自动化、Monolith Widget编译保存重载、PIE／双客户端、RHI颜色测试、地图出生碰撞导航、Cook／Stage／Server产物剥离、设备适配与集中人工签审。所有编译型检查均遵守用户“不构建”约束，未用静态成功替代。

## 独立审查与后续门禁

按执行／审查技能要求安排一次独立只读审查，没有委托实现或运行构建。发现与处理：

- Important：同玩家同时持有邀请与申请时，第二项接受可改变终态却不推进Revision。已补`TestParallelInvitationAndApplicationAdvanceRevision`，实现保证每项Pending终态变化推进修订且不重复入队。只完成源码修补，测试未执行，不记RED→GREEN。
- Minor：非成员准备、踢出自己返回裸中文导致公开适配误映射503。已统一为现有`PARTY_NOT_MEMBER`／`PARTY_COMMAND_INVALID`稳定码并补领域错误码回归源码；传输层实际行为仍待执行测试。
- 同时补加入序号溢出不变更聚合、所有加入顺序重复拒绝、未知请求类型失败关闭的实现与回归源码；不是编译／测试通过声明。

审查明确不判断的已知未完成项均接受为后续发布阻断：队列统一事务与恢复、旧票据升级、真实Redis运行、在线状态与限频、状态订阅与阶段命令、协议互通及真实蓝图／颜色／Cook。它们没有因审查未额外提缺陷而自动通过，误判代价为重复报名、错误准入、过期UI或不完整客户端产物。没有将这些风险降级为可忽略的小问题，当前不进行提交、合并、发布或生产操作。

## 2026年10月14日续行启动结果

用户要求“继续执行”，随后进一步明确“请启动编辑器后执行”。本轮先启动正式编辑器，暂停进一步交互源码实施；本轮新增但尚未配套实现的Codec测试草稿已撤回，不作为交付，不改动既有源码／回归用例。原有“不构建”约束没有被明确撤销，未调用UBT、LiveCoding、Go构建或Cook，也未禁用必需插件绕过加载错误。

- 启动进程31840，实际程序`F:/UnrealEngine-5.8.0-release/Engine/Binaries/Win64/UnrealEditor.exe`，Build.version核对为5.8.0；命令行工程为当前正式`Game/DivineBeastsArena.uproject`，参数包含`-NoCompile -NoCompileEditor -NoHotReload -NoLiveCoding -unattended`。
- 新日志`Game/Saved/Logs/VillagePartyArenaEditor_Continue_20261014.log`：行1787—1788记录DBAWorldsRuntime预载失败和缺少GamePlatformInteraction导入；行2473—2477记录PresentationClient及UIClient预载／导入失败；行3164记录DivineBeastsArenaClient无法加载、DBAArena失败；行3234—3235记录11:18:10退出及11:18:11关闭日志（北京时间）。随后进程查询为空。
- 实际只读文件核对：GamePlatformInteraction的Editor DLL在正式插件和Game／Build／Tools已有产物中未找到；**DBAWorldsRuntime和DivineBeastsUIClient的DLL文件实际存在**。日志中的Missing import不能被写成所有这些文件均在磁盘缺失；已有文件仍可能因传递依赖或导入接口失败而无法加载。PresentationClient的错误127尚未完成导出符号级诊断，不把补一个DLL宣称为全部修复。
- Monolith状态仍返回`Unreal Editor not running`，未获得正式工程可操作的UI连接。本轮没有创建、编译、保存或重载蓝图，没有更新生成成功清单，累计真实新增蓝图仍为0。

在当前“不构建”且无匹配Editor模块完整产物的条件下，不能完成“启动成功后制作蓝图”。下一前提是明确允许仅对正式工程Editor目标必要模块进行一致构建／链接，或提供与当前工程匹配的完整Editor产物；之后须重新启动并核对Monolith，不能直接把旧设计规范改为成功状态。Client／Server构建、Cook及生产操作不包含在该恢复前提中。

## Editor必要构建与重启授权

用户随后明确允许“仅构建正式工程Editor目标所需模块，然后重启编辑器”。此授权取代上一阶段对Editor构建的禁止，不扩大到Client／Server、Go构建、Cook／Stage、发布或凭据操作。

Ruling: 使用既有`Build/Game/BuildFoundation.ps1 -Editor`执行Development／Win64真实增量构建，复用锁定UE5.8及其预编译引擎依赖，不清理旧产物、不修改BuildId、不禁用出错模块。发现其他任务已有Server构建进程53552时先等待，不终止或计作本任务构建；具体命令、运行目录、退出码及编辑器重启结果由后续实际证据补入，当前尚不记为成功。

本次预定RunId为`28059359-95a3-4606-b54c-ffd4d256b115`；调用为`Build/Game/BuildFoundation.ps1 -EngineRoot F:/UnrealEngine-5.8.0-release -Editor -UsePrecompiled -NoUBA -MaxParallelActions 2 -TimeoutSeconds 7200 -RunId 28059359-95a3-4606-b54c-ffd4d256b115`。启动前等待其他任务进程53552退出，随后由既有脚本取得Foundation锁并等待UBT互斥；11:25仍处于排队等待、本次UBT尚未执行，预定`Saved/Validation/FoundationM0/<RunId>/Build`目录尚未生成。此处仅登记真实调度状态，不是构建通过证据。

### 本次Editor构建实际结果

上述排队记录保留为历史过程。随后构建实际执行并结束：既有脚本退出码0，`Status=Passed`，`EditorBuildEvidenceStatus=Eligible`；唯一目标为`DivineBeastsArenaEditor`，Win64／Development，锁定引擎5.8.0。UBT实际进程17672执行247项动作，日志为`Result: Succeeded`，UBT耗时568.86秒、脚本记录的目标进程耗时570.1067099秒，未超时。没有启动Client／Server、Go构建、Cook／Stage或打包。

- 证据根：`Saved/Validation/FoundationM0/28059359-95a3-4606-b54c-ffd4d256b115/Build`；实际结果为`result.json`，目标日志为`Editor/UBT.log`。输入清单2694个文件，前后源码端点一致，SHA-256为`4E2A04080E387994C450C866A3998698B5D5531C3165BE564E7DA6B7F24338D1`；成功回执覆盖76个工程DLL，BuildId为`cf41249f-44dc-4956-bd09-4a42f079e86b`。这些是本地增量Editor构建证据，不代表所有DLL均被重新链接或其他目标已验收。
- 构建保留1条既有C4996弃用警告：`GamePlatformCombat/Private/Actors/GamePlatformCombatProjectile.cpp:25`直接访问`AActor::NetUpdateFrequency`，后续应单独迁移到访问接口并复核行为。本次不为清除警告扩大源码修改，也不宣称零警告。
- UBT按正常构建流程更新旧的派生lib／pdb等产物，未执行Clean、删除源码或删除资产。`-UsePrecompiled`复用既有引擎产物；`-NoUBA`只关闭Detour路径，不将其写成完全不使用UBA调度。
- 构建完成后以可见交互窗口重新启动锁定`UnrealEditor.exe`，进程48384，工程路径为当前正式`Game/DivineBeastsArena.uproject`，参数`-NoCompile -NoCompileEditor -NoHotReload -NoLiveCoding -nosplash`。新日志为`Game/Saved/Logs/VillagePartyArenaEditor_AfterEditorBuild_28059359.log`；12:00已加载GamePlatformUIClient／VFX等模块，仍处于初始化，尚不把进程存在等同于编辑器及Monolith就绪。

前文“不构建／UE三目标未执行”为本次授权前的历史状态。当前只新增Editor构建成功证据；原生自动化、客户端／服务器构建、Go测试、PIE、Cook及UI资产编译保存重载仍未执行，真实新增蓝图仍为0。

# 角色创建与选择页增量修正

## 本次要求及边界

在main原位修改，不创建分支，不提交或推送。保持唯一应用流程执行器、ViewModel事件更新、后端创建资格、三角色及三层依赖不变。本轮只修改公共UI内容及DBAClient客户端显示适配，不修改业务后端、地图、三维角色模型或服务器。

角色创建页左侧面板从268×322放大为536×805，宽2倍、高2.5倍；右侧从200×216放大为500×540，宽高均2.5倍。文字、姓名输入及既有按钮不随整页缩放。上述固定逻辑像素需1920×1080工作区；960×540只能用于登录回归，不能声称该小窗口可完整显示放大的创建页。前端验证启动器增加WindowWidth/WindowHeight，并默认1920×1080。

创建与选择页移除BrandTitle（中文文字）、BrandSubtitle（英文文字）及TopRule（横线），保留此前用户指定的BrandLogo图像。创建页生肖选项改为图像；保留忙碌禁用、选中反馈、中文悬停名称、未知资源文字回退及键盘按钮焦点。

## 鼠标旋转与资源合同

原生鼠标旋转命令仍存在，没有修改为自动旋转或Tick输入轮询。本轮将两页PreviewDragSurface显式设为命名变量、Visible且BrushColor透明，垂直锚点延伸至窗口底部并避开底部142像素操作区；中央宽800。装饰面板HitTestInvisible，选择按钮仍在更高层。仅按住左键的同一用户/指针可以旋转，释放、失活和失去捕获沿用原有清理。

公共生肖图集由DBAUIPack_Core拥有，运行时UDivineBeastsCharacterChoiceEntry只对既有Image画刷设置UV区域；不硬编码磁盘路径、不新增资源加载器、不直接访问HTTP。4列3行从左到右、从上到下为Rat、Ox、Tiger、Rabbit、Dragon、Snake、Horse、Goat、Monkey、Rooster、Dog、Boar。身份映射不依赖后端资格数组顺序。未知身份隐藏图像并显示文字；取消清除UV及可见性，不卸载内容资产拥有的图集。

原稿：SourceArt/UI/Zodiac/ZodiacChoiceAtlas_V1.png，1448×1086，每格362×362，SHA256见ZodiacChoiceAtlasImportManifest.json。生成工具为内置image_gen，图像为碧绿温润抛光玉石生肖；不是现有彩色人形占位肖像，不代表正式三维英雄已经制作。PNG字节不裁切、不重绘、不调色。Monolith导入为sRGB、TC_EditorIcon、UI、NoMipmaps及无额外色调调整，显示白色乘色。

实际生成提示词：

> Use case: stylized-concept. Asset type: one finished UI icon ATLAS texture for the original Chinese mythic game Divine Beasts. Create a precisely aligned atlas with FOUR equal columns and THREE equal rows, exactly twelve square icon cells, no gutters, no text or numbers, each square same size. Landscape 4:3 canvas, prefer 2048x1536. One large easily recognizable zodiac animal sculptural bust centered within EACH square, with 12% safe margin per cell. EXACT row-major order: top row RAT, OX, TIGER, RABBIT; second row CHINESE DRAGON, SNAKE, HORSE, GOAT; third row MONKEY, ROOSTER, DOG, BOAR/PIG. Every animal clearly distinct, no people or mannequins. High quality AAA cinematic 3D render, elegant fine-polished luminous jade in natural emerald, celadon and warm pale green tones, restrained flowing leaves accent at base, small subtle gold accent only. Temperament ancient Chinese nature totem, but surface absolutely clean, smooth, warm, translucent, expertly polished; broad continuous soft highlights, gentle realistic light, subtle low-contrast natural jade variation. Dark deep-emerald perfectly smooth background identical across all cells. NO granular texture, film grain, sand, noise, cracks, chips, weathering, dirty speckles, dense fine engraving, excessive sharpening, glitter, broken highlights, white overexposed outlines, neon, lettering, frame, border or watermark. The twelve cells are a single production sprite atlas, not a presentation mockup. Keep animal busts approximately equal apparent size; readable at 96-pixel cell display. Uniform near frontal three-quarter angle in all cells.

## 修改、回退与验证

资产修改唯一入口为CharacterEntryRevisionMonolith.py的Monolith请求；只增量修改指定节点，不重建或覆盖其他页面。--apply须在尚未执行的备份目录运行，拒绝覆盖既有备份；不用于无审查重复放大。生成器同步固定基线，避免再次生成恢复旧尺寸、旧文字和匿名拖动区。

Saved/Validation/CharacterEntryRevision20261011/Before保存三个Widget修改前副本及哈希；Actions.json保存58项实际动作返回。三份Widget初次编译均0错误/0警告并保存；图集设置/原稿哈希阻断审计成功。真实布局门禁修改前退出1，修改后退出0；该项不是GPU或客户端验收。

补充原生自动化用例ZodiacImageChoice（12格倒序投影、文字回退及取消）、PreviewSurfaceBinding（两页真实绑定）；保留ChoiceIdentityLifetime与PreviewDragLifetime。SourceToSlate真实RHI颜色用例增加十二格图集取样，前端容器检查新增正式图集必需项。未加载用例或未运行不记通过。

构建首次退出6：当前锁定引擎的NetCore、EnhancedInput、PCG导入库缺失，以及新增测试漏UPackage包含/错误访问Slate废弃包装类型；测试错误已修正。从现有DLL的实际PE导出表重建三份缺失导入库，函数/DATA按真实节属性区分；三者BuildId均49444017-86c4-46bd-a6ce-36b57136a1f9，DLL SHA256生成前后不变。不改引擎DLL、版本、BuildId或模块装配。证据位于Saved同目录ImportLibraries。

二次构建发现新增GameplayTasks与DataValidation导入库缺失，并定位本轮临时DEF中的C++符号引号被MSVC保留为字节。恢复扩展为五份导入库，按本轮生成哈希核对后仅修正自有库；dumpbin验证真实__imp_符号不含引号。所有原DLL哈希仍未变化，证据为ImportLibraries/Evidence2.json。

Editor目标在`de6e3b84-2e71-4d41-878c-5a856f80d9d2`真实构建退出0。该次随后Client使用UsePrecompiled失败：单体目标没有相应预编译清单；不复制或伪造清单，改用既有Client正常源码构建入口（不指定UsePrecompiled）。独立重开正式工程后，Monolith再次编译三页0错误/0警告、保存并回读布局门禁通过。实际加载并运行角色前缀4项原生用例，4通过、0失败、0跳过，包含十二格图像投影及两页真实鼠标命名绑定。

Client正常源码构建`519a94a1-d3c5-49ae-a5e7-9f774f06dc0c`退出0，实际编译并链接新版客户端；未降低引擎或关闭模块。真实RHI运行`96f535bc-c0a4-4dcb-aad5-b8d7c6eb2912`退出0，实际找到SourceToSlate一项且Success，包含新图集十二格取样及五份单次编码PNG；当前UI/原稿指纹为A64F5856E4F4DD427F2AB6FE92094AFED45A5EA86D8FBAE46438FC8D2FAF162A。报告独立复查后才能用于发布。启动日志还出现测试队列之前的Condition failed记录，不将整个日志宣称零错误；本项断言结果以实际Automation报告为准。

头文件880处引用无缺失，三层Public扫描510头文件/1101类型/194边通过；颜色导入策略3项通过，前端预览架构脚本通过，修改差异卫生检查通过。静态计数不冒充GPU、Cook或联机结果。

正式FrontEndClient配置重新Cook/Stage：`4730eb8d-e8ed-423c-b598-f31133b37d91`退出0，当前RHI颜色发布门禁通过；实际IoStore必需资源135项无缺失，生肖图集有非空ExportBundleData；既有鸟群19项容器门禁也通过。未复用旧Stage或绕过资源检查。

两个新版客户端运行记录为`668d0ba4-e3bc-4dc6-bdb4-e02fa9a46077`，PID17700/31188，精确可执行文件及UTC启动时间匹配、两进程存活，AutoLogin=false。两份实际日志均记录WBP_DBA_UI_Login为当前输入叶节点。启动窗口1920×1080，不传开发账号或密码；业务后端五个就绪接口返回200，既有Village专服保留。Windows桌面随后核实为锁屏，激活窗口返回GetCursorPos拒绝访问；没有解锁或操作安全界面，没有把锁屏截图当成游戏渲染证据。

尚未完成：新版角色页实际鼠标旋转、最终图像/面板视觉及人工登录到Village联机验收。自动原生、RHI和Cook通过不替代这些验收；用户解锁并手动登录后可继续操作验证。

中文说明审核范围为图集来源、Monolith工具、命名控件、显示/回退/取消、验证启动参数与测试，不宣称全仓中文注释已审计。并行Fauna/鸟群及其他未提交改动保留。

## 登录入口、鼠标方向与介绍补充（2026-10-12）

用户追加要求：登录后进入选择角色页；修正反向鼠标旋转；桃花落地不弹跳，地面少量落叶；创建角色显示从DataAsset加载的角色介绍和技能介绍。本轮直接在main原位增量修改，不提交、推送或改动未在本轮范围内的代码。

该阶段曾采用“包括空角色账号也默认选择页”的方案，已被下节2026-10-12最新要求取代：无角色且允许创建时直接打开创建页，有角色打开选择页，取消仍可返回空列表。允许查看页面不等于允许提交角色选择，认证/忙碌/加载/错误及后端权威验证仍分开检查。共享鼠标手势按右拖负Yaw、左拖正Yaw映射，保留指针所有权、非有限值过滤、90度单事件上限和失活取消。

已有桃花地面模块根据阵风生成向上速度，不是刚体反弹。只将项目桃花两层地面WindStrength改为0，空中风及平台通用模块、其他三个叶片风格保持原状。正式工作室增量保存40片竹叶/银杏叶，位置限定在已批准圆台石面，保留角色、镜头和PlayerStart；不使用隐藏Landscape高度代替可见圆台。重载检查曾发现运行时禁用碰撞被BlockAll恢复，已持久化NoCollision配置和Actor碰撞开关，保存重载后的无碰撞门禁通过。

右侧500×540面板内移除写死旅行提示，改为独立410×260滚动介绍区。动态HeroIntroductionText、SkillIntroductionText和HeroOverviewStatusText只接收VM快照，字体14/12、标题16；不缩放整页、不改变生肖图集RGB、不覆盖中间旋转和返回按钮。英雄介绍唯一作者来源为HeroDefinition.Introduction及源JSON；技能介绍使用独立CharacterCreationProfile主资产类型，不重用战斗UIProfile身份、不把/Game/Development技能资产拉入前端发布包。当前显示的技能仍为开发候选，必须明确未获正式技能批准，介绍不产生授权。

验证进度：Editor RED构建70b4bb68-dfc5-4e1a-973e-cfbf1f020976退出0；实际运行路由6项（4失败）、拖动3项（3失败）、介绍3项（3失败），均复现目标缺陷。真实介绍布局门禁先失败后通过；桃花/地面门禁先失败，修复并重载后通过，两层GPU编译0错误/0警告。全量Tests/Architecture的80项Pester已通过。新介绍数据、最终原生回归、最终Editor/Client构建、RHI、Cook及客户端发布尚待本轮继续执行；以上不代表人工动态或登录新手村联机验收。

十二份英雄和十二份独立创建Profile已由Monolith正式保存；创建页重开编译0错误/0警告，HeroIntroductionDataAssets原生测试成功。首次新增源码编译暴露成员遮蔽、FString误用ToString和UHT不完整UniquePtr类型三项问题，均按真实类型修正，Editor 604e04aa-dae4-4e42-bb92-4cafcbfc73de通过；不更改严格编译设置。

完整UI原生回归首次实际54项：52无警告成功、1带警告成功，空列表焦点失败。排查确认Transient夹具缺真实Slate和明确焦点配置，补完整前置后再次54项：52无警告成功、1带警告成功，唯一同步Loading换VM回归失败，新VM角色仍None/Empty；首次该回归在后续测试源码校准前通过不能作为最终版本RED。第二份报告45754abe-6c13-44c5-a760-ca85eece33d7才是最终可复现证据。带警告项为PlayerStatusSnapshot的GameplayCue扫描路径回退，不将其丢失计数或宣称全日志零警告。

最终生产补刷修复采用最多两轮同步投影与合并弱任务；换VM立即终止旧轮次，失活代次取消旧任务，不使用业务Tick或无限递归。Editor构建30207b8f-1d8a-42d4-8373-617acfd13dcb与Client正常源码构建c554ecb0-f711-44fd-b190-6643bb5d69be均退出0。实际UI报告83e4ba6e-4408-41bc-8617-5cc3eccf21bf的54条均Success（53无警告、1带警告），P2最终夹具保持不变并GREEN；实际中央Data租约切换/失活用例执行，不以替身或零项代替。

独立重开正式编辑器PID48512后，Monolith0.23.0核对正式uproject和工作室地图；再次编译两页均0错误/0警告并保存。ReloadAudit.json确认真实滚动控件、十二份Hero.Introduction及十二份Profile/六十项正文与作者源一致，已保存地面40片叶片和桃花无扬起合同通过；脏包0，编辑器正常退出0。此报告不证明人工动态运动或最终视觉。

纯介绍源负向回归11项、全Tests/Architecture 80项均通过。最终真实RHI25289496-e5fd-479c-9c22-ced922f676b5找到SourceToSlate且Success，五份单次编码PNG存在；当前UI/原稿指纹75A039B30B17944B2E0D3DC6DC1DC5B4291B541E0EBE93CD37876C32A789ACE2。头文件902处无缺失，Public边界512头文件/1105类型/195继承边通过；英文命名122269项/64插件零违规，前端地图端侧检查通过。静态扫描不替代真实资产或Cook。

正式FrontEndClient重新Cook/Stage 1f547f9b-9aff-4148-96f0-21a637c5e7a7退出0，未复用旧包。147项必需非空ExportBundleData无缺失，包括十二份新增CreationProfile；既有鸟群19项容器门禁也通过。业务后端五个ready接口返回200，既有Village服务器PID31532保留；未因本地介绍扩展修改技能权威或重新部署服务器。

新版两个客户端运行记录bce54148-c10c-447c-a3bb-5cd04364249c，PID26812/1248，精确exe路径和UTC启动Ticks一致且存活；AutoLogin=false，1920×1080，两份真实日志均确认WBP_DBA_UI_Login为当前输入叶节点、焦点AccountInput，不自动提交账号密码。StartupVerified.json保留独立身份和首屏结果。启动日志仍有既有Boot页面加载警告：上轮668d0ba4的Client1.log亦有同样记录，本轮登录首屏随后成功；没有把启动日志宣称零警告，也未在本介绍任务中扩大修复该存量问题。

仍须人工登录后检查长介绍滚动、十二角色切换、实际拖动方向、桃花静止落地与视觉，以及登录到Village联机验收；原生/RHI/Cook成功不等同这些人工动态步骤已完成。本轮未提交、推送代码，未完成全仓中文说明审计；并行提交及其他原有改动保留。

## 进入游戏、纯档案列表和新玩家入口修订（2026-10-12）

本轮验证期间主分支更新为`878f0608`，该提交的六个界面源码文件残留合并冲突标记，Editor构建在`DivineBeastsUIClient`失败。先只消除该模块的冲突：保留名称/说明拒绝空白的资产校验、负方向拖动、创建页介绍依赖及取消草稿门禁；UI子系统实际使用两种开发模式，保留两份必要头文件。没有回退其他变更，也没有把编译错误当作预期业务回归失败；清除后重新构建、执行原生回归。

最新用户指令覆盖本文件此前“空档案也默认选择页”的决定：认证与档案加载完成后，无角色直接打开创建页，有角色打开选择页；LoadRoster阶段的未知列表不等于新玩家。打开创建页不产生业务角色或授权，创建仍经唯一应用流程和后端校验。

Computer Use实际读取正在运行客户端：空角色提示显示，右侧旧下拉框为空，底部“进入游戏”被禁用，中央仍有占位模型；此前没有切服日志。该证据证明当前空账号入口的误导性，不能声称已有角色的后端准入已失败。角色选择改为ScrollBox/VerticalBox真实档案列表，去除旧ComboBox及其隐藏索引依赖；本地浏览身份只表达选中UI，不代替后端正式角色选择。进入游戏提交当前有效且可用档案，继续执行ValidateSelection→ResolveExperience→RequestWorld→TransferWorld，不直接Travel或伪造准入。

无角色默认创建，人工取消回到空选择页时，主入口显示创建引导；忙碌、未认证、命令撤销和禁用档案仍阻断。登录背景框、两个输入、按钮、密码显示和反馈整组按固定基线上移60逻辑像素，保留大小、字号、颜色、原稿RGB和底部锚定，不重导入纹理。

真实资产门禁先复现旧ComboBox及主按钮文字绑定缺失。冲突清除后的完整Editor构建`4ea25a71-9c57-44ce-8a79-8e86b258fe59`成功；实际原生RED报告`Native-4a10801b-3cb5-454c-bd40-7716b276b320/Report/index.json`复现两项业务断言失败：无档案仍显示选择页、空列表主按钮禁用且文案错误。HWeb外层Job在未显式传播子进程退出码时显示0，并不表示测试通过；本轮依据引擎实际报告判定失败，后续命令显式传播退出码。

新增主按钮真实点击回归使用原生按钮委托、VM及无Contract的真实Owner：期望同步拒绝并显示错误，而不是伪造后端成功。最终GREEN、Monolith编译保存重载、RHI与新版Cook及启动结果见下文独立记录；不得沿用早前发布结果。本轮仅改对应UI/路由/作者工具/测试和中文说明，保持main及其他未提交修改。

Monolith本轮15项增量动作已完成，Select/Login均新鲜编译0错误/0警告并保存；没有重导入RGB或改变透明度合同。独立重开编辑器PID48084后再次编译、保存及读取真实控件树，通过纯列表/命名绑定/登录固定尺寸与位置门禁。动作和备份位于`Saved/Validation/EnterGameRepair20261012/Assets`，重载证据位于`Reload-363db08c-06c8-4e77-a719-63099d2693cb`；实际ChoiceEntryClass仍指向正式`WBP_DBA_CharacterChoice_C`。公共内容包的MonolithGenerationManifest记录同一证据。

静态头文件966处无缺失，Public边界528头文件/1140类型/197继承边通过；前端地图端侧检查通过。完整架构Pester实际80项中79通过、1失败：`DBAPluginConsolidation.Tests.ps1:66`的固定模块集合尚未包含主分支已有`DivineBeastsDemoClient`；登录UI门禁5项均通过。没有修改并行新增Demo模块或放宽固定集合来隐瞒失败，不能宣称全仓门禁全绿。本轮中文说明覆盖分流、浏览身份、提交资格、事件/失活、测试输入与失败证据；不代表全仓注释合规。

两个上一轮客户端PID26812/1248已不存活；其HWeb所有者Job已lost，停止请求没有影响后端或其他客户端。五个后端ready接口实测200，Village服务器PID31532身份及UTC启动时间匹配。另有两项其他会话的持久运行Job，未停止它们。最终发布仍须创建本轮独占客户端并核验实际登录输入叶节点。

最终Editor目标`465c94cf-0e9d-48f0-91fe-f090cf4f26b4`实际退出0，完整输入/receipt证据Eligible。修正测试夹具后，真实原生UI报告`Native-ac804e5d-aec5-4c90-a4ba-80676f25189c/Report/index.json`共62项Success，其中61项无警告、1项带警告、0失败；三个必需新回归实际存在且成功。警告来自`Combat.PlayerStatusSnapshot`的既有GameplayCue扫描目录回退，不将整个日志宣称零警告。

真实RHI运行`f1b70b54-1c00-46b4-82de-c01357a4e9d5`实际找到`DivineBeasts.UI.Color.SourceToSlate`且Success，五份单次编码PNG及当前UI/原图指纹`77D9F9F5CE4E8889A7C805D0028D9CAA2DB4EA9C84E60AAFA69DDD1D0C2049D0`一致。读取1920×1080实际RHI图确认登录框与命中控件整组上移；这不是已经手工认证的客户端截图。

独立只读复核未发现阻断项。剩余测试覆盖限制：无Contract拒绝路径证明真实按钮到VM/Owner且显示错误，但没有独立观察命令类型和角色ID；错误码相同不能单独排除误接其他命令，生产提交类型/ID另经源码读取确认。此项保留为测试增强，不声称已验证后台准入成功。

客户端完整源码构建`74f76c02-5958-4ad9-9719-383695a5c6a4`退出6：唯一编译失败位于引擎`Module.Renderer.20.cpp`，MSVC 14.44报告C1001并以访问异常退出。仅重试既有正式Client构建，单次并行动作从8改为4；没有修改引擎源码、切换工具链、禁用优化或关闭模块。独占重试`10c7011d-9091-4243-ae45-1d34f0e034d9`实际执行剩余3项编译/链接动作，49.23秒后Succeeded、退出0。不将一次重试成功推断为已找到编译器崩溃的确定根因。

正式FrontEndClient配置重新Cook/Stage `df71fe83-f12b-4e8e-9db2-dc260e8756dc`实际退出0，当前RHI颜色发布门禁通过。独立检查最终IoStore中的147项必需前端资源、19项鸟群依赖及4项P7音频内容依赖，均无缺失或禁入项；不复用早前发布包。烘焙产生的`Game/Build/WindowsClient/FileOpenOrder/EditorOpenOrder.log`变更保留，没有为清理状态而覆盖其他修改。

人工验证客户端启动记录为`Saved/Validation/FrontEndClients/e8a05798-25a3-46e7-bb38-7e6c3ac341ea/Clients.json`：两个PID为65236/65336，UTC启动Ticks分别639274045501990069/639274045502269452。精确exe均来自上述新Stage，1920×1080、每端60帧上限、AutoLogin=false；独立复查身份一致且存活，两份实际日志确认`WBP_DBA_UI_Login`为输入叶节点、焦点`AccountInput`，未检出Fatal/Critical启动错误，不将其他类别日志宣称零警告。

Computer Use取得两个独立真实游戏窗口60690084/4395000；首次非前台捕获返回其他前台画面，不作为游戏证据。随后分别激活已核对的游戏窗口并重新读取，两者均实际显示空白用户名/密码和登录按钮。未输入账号密码、未点击登录或进入游戏，不把登录首屏视觉当作认证或新手村准入通过。五个本地ready接口再次实测200，既有Village专服PID31532及UTC启动Ticks639273290401015426匹配；其他会话的持久进程保持不变。

交付为两个保留运行的新版客户端，供人工分别验证“无角色账号→直接创建→新手村”及“有角色账号→档案列表→进入游戏”；拖动方向、介绍滚动与最终动态视觉同样待人工验收。完整架构门禁的上述1项存量失败未被隐藏，联机认证/入服未由本次启动操作代验。本轮未执行提交或推送；主分支并行提交不计作本轮代理提交。

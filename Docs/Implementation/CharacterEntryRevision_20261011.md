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

选择页是角色入口的默认页面，包括空角色账号；点击创建才打开表单，取消可返回空列表。允许查看页面不等于允许提交角色选择，认证/忙碌/加载/错误及后端权威验证仍分开检查。共享鼠标手势按右拖负Yaw、左拖正Yaw映射，保留指针所有权、非有限值过滤、90度单事件上限和失活取消。

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

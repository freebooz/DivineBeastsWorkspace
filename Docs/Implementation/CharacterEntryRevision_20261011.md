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

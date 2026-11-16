# SSLM 金边云纹通用按钮 Implementation Plan
> For agentic workers: use superpowers:executing-plans; execute serially with the shared editor.
**Goal:** 将用户参考图中的绿色确认、海蓝取消按钮提取为无字、透明、可调色的正式主/次按钮样式。
**Architecture:** GamePlatformUI 现有 LocalPlayer 主题解析、事件绑定及 CommonUI/UMG 交互继续复用；DBAUIPack_Core 持有源图、纹理、材质、实例与两份既有按钮样式；DBAClient 现有 UI.Style.Button.Primary/Secondary 映射消费同一套资产。无需新增 MOBA 扩展或 C++ 执行器。
**Tech Stack:** UE5.8、Monolith MCP、UMG/CommonUI、UI材质、imagegen、PowerShell尺寸导出。
**Spec:** 本文件下述设计合同与用户本轮参考附件。

## 设计合同
- 绿色主操作、海蓝次操作；金边、云纹、圆端形制与参考一致。文字不烘焙，现有本地化文本独立显示。
- 用户最终确认213×60：透明PNG与原生默认显示尺寸均为213×60，不再将图片尺寸折算成64×18。母图640×180保留以适配DPI；九宫格边距L/R=.16、T/B=.44。现有页面的固定布局与字号不批量改动，通用样式提供默认尺寸。
- 两类各四状态：正常、悬停、按下、禁用；选中沿用悬停/按下，保留原输入、声音、焦点与业务命令。
- 单UI母材质，两次纹理采样（sRGB彩色图及线性Alpha图）；FillColor、RecolorAmount、Brightness、Saturation、Opacity可调。金色区域不参与局部换色。
- 默认RecolorAmount=0，原图颜色直通；不增加Tick、常驻计时器、模糊、SceneColor或独立播放器。
- 所有.uasset由真实引擎创建/编译/保存/重载；源图与脚本完成不等于原生验收。

## 执行任务
- [x] P0：读取现行规则、分支和脏文件；确认已有主题/样式及实际挂载目录。
- [x] P1：imagegen分离两份无字透明按钮底图。
- [x] P2：两份640×180源图及透明缩小版本已导出；当前213×60，历史320×90保留供比较。
- [x] P3：4份纹理、1份母材质、8份状态实例及1份换色示例已原生制作；两份既有样式已备份。
- [x] P4：既有CommonButtonStyle的7个状态字段已接入，正常/选中/禁用均读回通过；主题和业务代码未修改。
- [ ] P5：两类四态、九宫格、换色和原生禁用标志已核实；实际鼠标悬停/按下/点击及禁用不触发，保留人工输入验收。
- [x] P6：18个原生包保存、reload后重新读取通过；真实RHI截图及原稿颜色比较已完成。

## Review Focus
- 透明边缘黑边/杂色：在深浅及青玉底上检查Alpha。
- 金边随填充色变色：红/紫色FillColor测试，金边仍保持暖金。
- 九宫格拉伸：142×40、213×60、300×60检查圆端与边框，并检查文字可读性；默认213×60。
- 禁用仍响应/双击回调：原生输入与点击计数验证。
- 字体与交互回归：仅替换背景状态画刷，不改已有页面字号、命令或文字样式。

## 执行裁定与门禁
- Ruling: 使用当前真实工程的原生资产工作流，限定两份既有样式和新按钮专属目录；共享main有其他未提交任务，保持分支和既存更改，不执行提交/重置/工作树切换。
- Ruling: 用户要求直接执行且此前已退出反复问询，按本合同制作后交人工视觉审核。
- Ruling: 共享编辑器当前由头像栏任务使用，P3及之后串行等待释放；源图和作者脚本先完成。
- 本轮为UI内容变化，无C++目标重编译需求；Cook/打包/双客户端验证如未实际执行单列未验收，不能以原生预览替代。

## 原生实施结果（2026-11-03）
- UE5.8正式工程：`Game/DivineBeastsArena.uproject`；本次专用编辑器PID52084、Monolith0.23.0/9316。启动和原生预览回读为t.MaxFPS=0、r.VSync=0。
- 平台现有GamePlatformUI主题事件和CommonUI/UMG状态机制继续使用；全部正式新增内容位于`/DBAUIPack_Core/UI/`。无C++/配置/服务器逻辑修改，无Tick、轮询或第二套按钮框架。
- 纹理：`Textures/Buttons/T_DBA_UI_CloudButton_{Primary|Secondary}`及对应`_Alpha`。源640×180保留高DPI细节，默认Slate画刷213×60；sRGB彩色与线性Alpha分开导入。
- 母材质：`Materials/Buttons/M_DBA_UI_CloudButton`；UI/Translucent、两次采样。原生材质编译返回成功，当前统计70条像素/47条顶点指令；未测GPU毫秒，不据此宣称帧率达标。
- 实例：`Materials/Buttons/MI_DBA_UI_Button_{Primary|Secondary}_{Normal|Hovered|Pressed|Disabled}`；另有`MI_DBA_UI_Button_Custom_Normal`紫色示例。
- 既有样式：`Styles/BP_DBA_ButtonStyle_Primary`、`Styles/BP_DBA_ButtonStyle_Secondary`。保持原路径和主题语义，不新增竞争映射；白色乘色、Box九宫格，选中状态分别复用Hovered/Pressed。
- 审核控件：`/Game/Development/UI/WBP_DBA_CloudButtonShowcase`；40个原生节点，构建零错误、零警告。`BP_DBA_CloudButtonCapture`仅作Development中的浮点RHI捕获夹具，不接正式HUD。
- 18个包保存与引擎reload后，尺寸、纹理sRGB、采样引用、母材质、状态值和画刷绑定再次验证通过；不是跨进程冷重开声明。
- 最终截图：`Saved/Validation/GenericCloudButtons20261103/NativeStates_213x60_sRGB.png`。真实WidgetComponent/Slate渲染至PF_FloatRGBA，UE ReadRenderTarget调用ToFColor(true)一次编码；随后仅无损封装PNG，没有色调、亮度、锐化或缩放处理。
- 原稿对RHI颜色：两类共10个采样，包括内芯、云纹和亮部；按Slate九宫格像素对齐、源sRGB解码、双线性采样及Alpha合成计算，最大线性误差0.002983，门槛0.008。证据`SourceToRHIColor.json`。
- 初始Monolith widget截图存在额外Gamma，不进入最终验收；初始整图UV比较未考虑九宫格顶点取整，修正验证坐标模型后通过，未改图或材质补偿。
- 四条UIColorImportManifest登记已受控合并，合并时保留其它18条；`ColorManifestMerge.json`记录前后哈希。生成清单和真实导入阻断审计已在后续串行批次完成，独立复核如下。
- 正常退出编辑器前DirtyContent/DirtyMaps均为空；临时Entry预览改动已清理，没有保存引擎地图，没有关闭当前游戏客户端或服务器。

## 使用与换色
通用页面继续绑定`UI.Style.Button.Primary`或`UI.Style.Button.Secondary`。文字独立、本地化和点击命令继续由现有控件处理；已有页面固定布局和字号不批量改动。

| 参数 | 用途 |
| --- | --- |
| `ColorTexture` / `AlphaTexture` | 同一按钮的sRGB彩色和线性Alpha资源，需成对选择 |
| `FillColor` | 内芯目标颜色（LinearColor），金边被遮罩保护 |
| `RecolorAmount` | 0保留原图RGB，1启用完整内芯换色 |
| `Brightness` | Normal 1、Hovered 1.12、Pressed 0.78、Disabled 0.62 |
| `Saturation` | Normal/Hovered 1、Pressed 0.96、Disabled 0.12 |
| `Opacity` | 正常三态1，Disabled 0.72 |

修改某一类按钮配色时，对该类四份状态实例同时设置同一`FillColor`及`RecolorAmount=1`，保留各自状态强度，避免悬停时恢复另一颜色。新增主题可复用此母材质制作实例，再通过既有主题映射绑定；不修改公共母材质或增加执行逻辑。

## 串行交接复核（2026-11-04）
- `ImportBlockingAudit.json`四条真实纹理审计全部通过，Errors均为空；该审计不承担GPU、Cook或人工验收结论。
- 现有`MonolithGenerationManifest.json`已追加`CloudButtons213x60_20261103`，18个唯一资源身份与本轮原生保存重载清单完全一致。该条目保留创建时的验证快照；后续Cook结论以本节及`HandoffVerification.json`为准。
- 统一UE5.8 Client Cook批次`d27c6fc3-6757-42b2-9c26-a43fe569e590`实际结果为Passed、ExitCode=0。已独立核对Cook结果和容器清单SHA256，与交接记录一致。
- IoStore导出清单按真实Filename映射核对：16项正式按钮资产均各有1条非空ExportBundleData，总计1,876,686字节；Stage中的客户端可执行文件和容器存在。2项`/Game/Development`审核夹具不计入这16项正式按钮资源。
- 独立证据：`Saved/Validation/GenericCloudButtons20261103/HandoffVerification.json`。本次仅复核和更新交付记录，未重启编辑器、客户端或服务器。

## 尚未验收
- 真实鼠标点击、悬停/按下事件、禁用不触发和键盘焦点：等待人工输入验收，不能把静态四态截图当输入测试。
- 新客户端Cook及按钮入包已经核实；尚未验证当前运行客户端已部署该包，亦未进行本轮真实客户端输入及网络验收。
- 最终视觉由用户确认。整体状态为原生资产实现及保存重载通过，不标记完整发布验收通过。


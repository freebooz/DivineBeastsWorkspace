# SSLM 插件化用户界面整改执行记录（2026-10-17）

工程：`DivineBeastsWorkspace/Game/DivineBeastsArena.uproject`；当前 `main` 来源基线 `bbd909b8`。本次仅实施独立源码/验证工具，保留其他会话正在编辑的登录WBP、加载纹理、NPC/PCG/表现资源。没有触碰Monolith专有的UI二进制资产、没有停止在运行的UE5.8 Editor/构建Job，也没有进行Git提交。

## 一、UI目录资产持续验证（P0 已修复源码与离线门禁）

原 `Tests/Architecture/ValidateGameUIComponentInventory.py`（游戏UI清单验证）以被Git忽略的`Saved/Monolith/GameUIComponentSpecs/Manifest.json`（一次性制作样板）为必需真源，导致现有真实WBP均存在时仍静态失败。本次改为兼容入口，调用新增`ValidateUICatalogNativeAssets.py`（真实目录与WBP合同审计）：

- 从C++ `DivineBeastsUIScreenCatalog.cpp`（公共页面目录）与 `DivineBeastsArenaUIScreenCatalog.cpp`（竞技页面目录）读取稳定页面身份和资源名。
- 以真实`DBAUIPack_Core`和`DBAArena`磁盘WBP包核对24项登记：18公共、6竞技；序列化包内核对预期原生父类名称，46个`BindWidget`（必需控件）名称全部存在。
- 检查独立RootLayout（本地玩家根布局）、版本化`MonolithGenerationManifest.json`（引擎制作证据索引），以及Editor/Server目标对客户端UI包的显式依赖边界。
- 保留`--require-legacy-specs`仅供复现旧32份声明式样板；缺Saved文件时显式失败，默认生产目录检查不再受临时目录影响。
- 审计结果只能证明当前源码目录、原生资产文件及字符串身份；不能替代真实UE AssetRegistry反射、控件类、Blueprint编译、焦点和Cook。

## 二、角色外观职责（P1 已修改源码）

从`DivineBeastsUIClientSubsystem::SyncCharacterPreview`（项目UI角色预览同步）移除世界Pawn的外观组件创建及英雄外观直接应用。界面在退出选角/创建阶段仅关闭本地预览舞台。

正式世界客户端已有`UDivineBeastsCharacterAppearanceWorldSubsystem`（项目角色外观世界子系统）在Actor生成与World BeginPlay接管`ACharacter`挂接；`UDivineBeastsCharacterAppearanceComponent`（角色外观组件）订阅项目角色身份和就绪事件并异步应用表现。保留真实世界权威状态为唯一外观身份来源，避免UI旧代状态在Travel/换Pawn时触发额外装配。需要UE原生对首次生成、同步载入、复制身份晚到、换人/退出、普通Pawn例外进行回归。

## 三、设置预览安全超时（P1 已修改源码）

`DivineBeastsSystemSettingsViewModel.cpp/.h`（设备设置UI视图模型）把原每0.1秒轮询的Ticker改为相对单调时钟的15秒一次性回调；回调确认原页面代次和预览资格。调度器异常提前触发时重新安排剩余时长，命令同步重入时短延迟一次，到期恢复失败时以1秒单次重试。确认、主动撤回和页面失活依旧调用原`CleanupDeviceSettings`清除本页句柄，真实设备值仍只有`GamePlatformSettingsClient`（平台设备设置服务）修改。本次没有移动密码/用户数据或另建设置数据源。

重要边界：目前15秒自动取消仍由激活UI页面拥有；如果页面退出时平台取消失败，旧设计的错误日志仍不能证明最后成功恢复。要彻底保障UI销毁后的系统级回滚，需以后由`GamePlatformSettingsClient`单独实现并经安全验证；本轮不在该平台模块引入违反其“无Ticker”架构门禁的周期任务，不声称此风险已经闭环。

## 四、插件演示差距（P0 真实覆盖率尚未达标）

新建`Tools/Unreal/PluginDemos/AuditPluginUIReadiness.py`（67插件能力审计）和`TestPluginUIReadiness.py`（统计反例测试），只读取Schema2机器台账、独立输出逐插件工作单，不更改正式编目或伪造演示：

- 67个插件：48代码/机制插件、19内容包；
- 全能力审查尚未完成：63项`unreviewed`（未审查），4项`partial`（部分完成）；
- 已登记4项能力、9个场景；接线4项；人工审核通过0项；
- 逐插件功能真实分母仍未知，`coveragePercent=null`；`--require-ready`用于显式发布门禁，对上述状态应拒绝通过。

现有新演示中心`DivineBeastsDemoWorldUI.cpp`（正式演示界面桥）已经通过平台LocalPlayer UIManager和层栈开屏。旧`DivineBeastsDemoWorldSubsystem.cpp`在开发兼容场景仍使用`AddToViewport`，并非正式客户端常驻调用。本批没有覆盖其旧WBP或删除旧接口；迁移须在当前开发地图实际Widget树/输入行为完成验证后进行。

## 五、实跑静态检查与证据等级

- `ValidateGameUIComponentInventory.py`：公共18、竞技6、已保存资产父类符号24/24、必需控件46/46，0缺失，退出0。
- `TestUIPluginRemediation.py`：7项通过，包含正常目录、重复身份、缺失WBP、非法父类、必须/可选控件、UI不得新建角色外观、预览15秒到期合同。
- `ValidateUIThemeIntegration.py`：20项主题静态合同，0失败。
- `AuditPluginUIReadiness.py --summary`：正确报告67/4/9/0、63/4，未知总功能分母保持null。
- `TestPluginUIReadiness.py`：5项通过；未审查、已接线未审核、未知案例引用以及伪造作者审查批准状态，都不能报发布成功。Schema2正式作者数据只允许`not_reviewed`；本审计绝不消费作者字段作为人工签收证据。
- `ValidatePluginDemoManifest.py --check-docs`：`issues=[]`，说明编目与派生文档一致，不代表Native案例能运行。
- `Tests/Architecture/TestUIDeliveryInventory.ps1`（另一份既有UI静态台账）已从历史22项补入Settings和WorldJournal两项；现有24/24源目录资产、10项领域基类、8份不可删除基础WBP及纹理检查通过（0缺失）。原生父类/必须控件名称另由新Python门禁严格审核。
- 修改路径Python语法检查及受影响Git差异检查通过。

当前没有执行新增C++的UHT/UE5.8编译、原生自动化用例、Monolith WBP重新编译/冷重载、真实键鼠手柄/移动端适配、生产Client/Server Cook及人工视觉/颜色验收；不把源静态门禁当作这些成果。

## 六、后续重点实施

1. 在独占、已保存且版本匹配的正式UE5.8 Editor环境中，运行UI层级/角色登录/系统设置安全重入回归，并验证最新Monolith工作区各WBP结构、焦点、可访问性和编译状态。
2. 将插件逐个归类为有视觉/交互样板、无自有界面但有可观察API、纯服务器/内部能力，审核真实功能分母后再补正式案例与Monolith样板资产；平台不能反向依赖项目资产。
3. 完成系统设置预览在UI页面销毁时回滚失败的服务级保障，随后做真实分辨率/窗口模式安全性和客户端/服务器资源剥离回归。
4. 以正确sRGB/单次编码的原生RHI或正式客户端截图复核玉石UI，禁止Monolith0.23.0有额外Gamma的旧widget捕获路径；最终人工签收单独记录。

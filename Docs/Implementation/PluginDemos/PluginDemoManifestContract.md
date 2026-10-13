# SSLM演示编目Schema2与UE定义接入说明

本合同用于项目层开发演示，不新增插件或玩法执行器。人工作者源为同目录`PluginDemoManifest.json`，字段合同为`PluginDemoManifest.schema.json`；派生覆盖表、地图表与后续UE资产不得独立维护另一套事实。

## 当前交付边界

- 当前编目登记47个代码插件、18个内容包和37张开发地图文件候选。65个插件范围均保持`unreviewed`；功能/案例/用例暂为空，不能显示100%或据此认为插件没有功能。
- Schema2验证器、中文派生文档导出和测试已经实现。验证器只证明结构、身份集合、路径和关系，不验证源码符号语义、资产可加载性或人工签收。
- `UDivineBeastsDemoCatalogDefinition`及其Private校验代码已经写入。**尚未构建或运行UE测试；尚未创建DataAsset，既有运行菜单尚未切换到此定义。**
- 未修改旧Widget父类、资产、默认地图、正式启动策略或业务权威。T03之后的Data加载、运行状态机、VM及Monolith界面仍是后续实施项。

## 作者源、历史与迁移

v1完整字段值保存在`History/PluginDemoManifest.v1.json`，保留其旧日期、47/17/31计数及`/Content`等历史记录，不把其修成“已通过”。v2的`history`记录保存位置、来源提交和存档文件SHA256；存档采用规范JSON排版，语义与迁移前一致，不承诺排版字节等同旧文件。

v2采用真实描述文件和内容登记更新身份。内容包的层级由旧`Content`转为`Project`，`plugin_kind=content_pack`仍可单独筛选；这不迁移任何稳定插件名。地图候选根据`Game/Content/Development/*.umap`映射为`/Game/Development/...`，不按路径猜正式功能所有者，也不将发现地图直接转为可运行案例。

旧调用方仍使用原地图发现目录。本文不声称已经切换运行目录；后续T03/T10必须用显式旧路径兼容表处理一对多选择，并同步WorldSubsystem及启动预检，不直接复制或改名UE二进制文件。

## 静态门禁与中文导出

在工作空间根运行（只读、不启动UE）：

```powershell
python -X utf8 Tests/Architecture/TestPluginDemoManifest.py
python -X utf8 Tests/Architecture/ValidatePluginDemoManifest.py --workspace . --manifest Docs/Implementation/PluginDemos/PluginDemoManifest.json --check-docs
```

退出0表示静态合同通过；退出1表示字段、关系或派生文档不一致；退出2表示命令行输入不可读取。`--print-document coverage`和`--print-document assets`打印对应中文Markdown，脚本本身不写文件。`--check-docs`对比同目录`FeatureCoverage.md`与`AssetReuseMap.md`，避免人工更新派生表而遗漏作者源。

门禁拒绝重复身份（按FName大小写不敏感）、缺实际插件、错层/错类别、错目标、未知引用、缺用例/证据、非法参数与超时、越界责任路径和歧义旧路由。只读检查保留原错误与输入对象，不自动修正数据、不调用任意文本命令。

Schema使用标准JSON Schema关键字的限定子集，由无第三方依赖的离线验证器消费：`$ref`仅引用本地`$defs`，以及`type/const/enum/required/properties/additionalProperties/items/minItems/uniqueItems/minLength/pattern/minimum/exclusiveMinimum`。新增关键字必须同时扩展真实执行器及回归，不能只改Schema宣称门禁已生效。

## UE派生合同与所有权

源码位于`DBAClient/Source/DivineBeastsDemoClient`：Public/Contracts只含稳定值类型，Public/Definitions继承平台`UGamePlatformDefinitionBase`；索引与完整关系校验放Private/Catalog。`GamePlatformCore`作为真实Public依赖提供中立结果，不反向引用项目。

| 作者字段 | UE派生字段/规则 |
| --- | --- |
| `plugins/features/scenarios/review_cases/legacy_map_routes` | Definition对应公开只读数组；`discovered_maps`仅供诊断，不能自动变为正式案例 |
| `schema_version=2` | `DataVersion.SchemaVersion=2`；CDO只接受2，内容修订必须大于0 |
| 作者源原始字节SHA256 | `SourceFingerprint`，64位小写十六进制；不是构建指纹 |
| `feature_id/scenario_id/review_case_id` | 分别映射独立FName；中文名称及地图不是身份 |
| 参数`type/default_value` | `DefaultValue.Type`及其唯一类型载荷；布尔、整数、实数、标识分开 |
| 参数`minimum/maximum` | 有限数值界限；数值默认值必须在界限内 |
| `map_asset/widget_asset/definition_assets` | FSoftObjectPath；合法不等于资源存在，后续由中央Data服务加载 |
| `run_status=idle/review_status=not_reviewed` | 作者初始状态，不写入定义的动态运行结果；T04/T08从真实运行/人工报告投影 |

整数参数只允许绝对值不超过`9007199254740991`的默认载荷，这是JSON数字/UE double范围比较的精度边界，不是游戏性能或内容数量预算。范围数值、布尔值、无穷和NaN必须明确区分，非法输入不截断、不夹紧为假成功。

定义不拥有Actor、输入状态、网络权威或Data租约。调用方仅在游戏线程检查/使用UObject；内容共享后只读，新的版本先完整检查再发布。完整运行租约与回调取消由后续现有目录子系统增量接入，不新增资产管理器。

## 未执行的验证与回退

原生`DivineBeasts.PluginDemos.Definition`覆盖结构版本、重复身份、同地图不同案例、超时/参数/操作/证据拒绝和旧路由选择；目前只有测试源码，用户明确禁止构建，所以未运行。离线23项回归不能替代UHT、Editor/Client/Server构建、UE自动化、资产重载、Cook或人工核验。

既有`ValidateDesignBaseline.ps1`因`Game/Saved/BirdFlockAuthorHost/BirdFlockAuthorHost.uproject`被递归计入而失败；没有删除该工程，也没有放宽门禁。实际日期与检查记录见`ExecutionProgress.md`。

回退仅撤销本次新增类型/测试及Build.cs公开依赖调整，使用历史记录恢复作者源与派生文档；当前没有激活新目录或修改二进制资源，不需要伪造资产回退。不得撤销其他并行任务代码或用清空工作树操作回退。

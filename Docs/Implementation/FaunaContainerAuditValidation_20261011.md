# 《神兽联盟》蝴蝶鱼群容器审计工具与验证记录

日期：2026-10-11；工程：DivineBeastsWorkspace（神兽联盟工作空间）。

## 实现归属
平台层 GamePlatformVFX（游戏平台视觉特效插件）负责通用运行；DBAPresentationPack_Core（项目公共表现内容包）负责物种视觉。审计工具不属于玩法权威模块。

- Tests/VFX/Fauna/ValidateFaunaContainer.py：对 UnrealPak 实际导出的多个 IoStore CSV 进行Client（客户端）与Server（专用服务器）资产检查。
- Tests/VFX/Fauna/TestFaunaContainerValidation.py：标准 Python unittest 测试夹具，不修改工程资源。
- Client（客户端）需包含平台4包与两物种各8包，共20个必须运行的资产包；Server（服务器）不得包含Fauna视觉资源、源资产和开发审核场景。
- 对每个容器独立指定一次 --csv，验证每行 Platform 与目标一致；空清单、缺字段、目标混用或遗漏资产均返回失败。

## 实际测试及边界
在锁定 UE5.8 Python 解释器执行 Tests/VFX/Fauna/TestFaunaContainerValidation.py，结果为11项通过、0项失败、进程退出码0。
真实覆盖：多容器合并、完整资源名单、缺包、Client开发地图或源资产禁入、Server错误资源、目标平台校验、空CSV、重复列、未提供CSV及路径识别。
这一结论只验证审计器规则。没有读取当前发行包的真实IoStore清单，因此绝不等价于Cook或Stage已通过。

## 后续真实产物检查
先生成带当前源码身份的 WindowsClient / WindowsServer Cook、Stage 和 UnrealPak CSV；再运行审计器，按实际CSV提交 --csv 与 --target Client/Server 及 --report 报告位置。
每个实际容器均需保存清单SHA-256、目标平台、唯一文件数、缺少的必需包及禁止包列表。
具体Cook计划见 Docs/Implementation/FaunaCookValidationPlan_20261011.md。

## 与实现进度对应
目前独立PIE报告 Saved/Validation/Fauna/IntegrationReview.json 显示10组GPU测试通过、鸟群与Fauna所有权检查通过、世界清理通过。通用群落行为17项原生测试通过。
Editor（编辑器）四模块构建已通过。Client/Server完整目标构建尚未取得本轮通过证据：缺失预编译manifest的快速构建失败，完整源码构建长时间运行后终止。
正式新手村花点/水域锚点绑定、真实Client/Server Cook/Stage和人工3A视觉验收均仍需独立验证。本文件不宣称生产就绪。

## 本次 WindowsClient 定向资源 Cook（后续实际执行）
- 实际目标：WindowsClient；范围仅为平台及项目环境Fauna两个内容目录；使用 NoGameAlwaysCook/NoDefaultMaps 控制根集，非正式整客户端发布。
- 执行器：UE5.8 UnrealEditor-Cmd Cook；日志为 Saved/Validation/Fauna/AssetCook_20261011T2256/Cook.log；进程退出码0、最终0 errors与0 warnings。
- 真实烘焙统计：Packages Cooked=568、Skipped by Platform=8、Total=576；Niagara GPU脚本编译计数=3。日志分别明确加载并编译 NS_DBA_ButterflyAzure、NS_DBA_FishSilver 与 NS_GP_Fauna_Base。
- 输出：Saved/Validation/Fauna/AssetCook_20261011T2256/Cooked/WindowsClient/DivineBeastsArena/AssetRegistry.bin 与 Metadata/DevelopmentAssetRegistry.bin 存在。
- 工程当前使用 Zen cooked output store（资源烘焙存储）；Zen manifest只有松散配置及元数据，不包含传统的全部.uasset文件。因此一次普通文件搜索为零不能证明Fauna遗漏；反过来也不能用上述成功计数证明全部20个必需资源都已打进最终Client发行容器。
- 剩余条件：读取Zen package oplog或以受控的 -skipzenstore 重新Cook取得loose package，再核实20个资源；正式Client/Server Stage与UnrealPak逐容器清单尚待执行。完整Client/Server C++目标在本轮尚未通过，不因定向Editor Cook退出0而视为成功。


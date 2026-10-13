# 《SSLM》蝴蝶鱼群容器审计工具与验证记录

日期：2026-10-11；工程：DivineBeastsWorkspace（SSLM工作空间）。

## 实现归属
平台层 GamePlatformVFX（游戏平台视觉特效插件）负责通用运行；DBAPresentationPack_Core（项目公共表现内容包）负责物种视觉。审计工具不属于玩法权威模块。

- Tests/VFX/Fauna/ValidateFaunaContainer.py：对 UnrealPak 实际导出的多个 IoStore CSV 进行Client（客户端）与Server（专用服务器）资产检查。
- Tests/VFX/Fauna/TestFaunaContainerValidation.py：标准 Python unittest 测试夹具，不修改工程资源。
- Client（客户端）需包含平台4包与两物种各8包，共20个必须运行的资产包；Server（服务器）不得包含Fauna视觉资源、源资产和开发审核场景。
- 对每个容器独立指定一次 --csv，验证每行 Platform 与目标一致；空清单、缺字段、目标混用或遗漏资产均返回失败。

## 实际测试及边界
在锁定 UE5.8 Python 解释器执行 Tests/VFX/Fauna/TestFaunaContainerValidation.py，结果为11项通过、0项失败、进程退出码0。
真实覆盖：多容器合并、完整资源名单、缺包、Client开发地图或源资产禁入、Server错误资源、目标平台校验、空CSV、重复列、未提供CSV及路径识别。
这 11 项仅验证审计器规则，不能代替实际 Cook/Stage 或容器清单。后续实际产物检查独立记录于下文。

## 后续真实产物检查
先生成带当前源码身份的 WindowsClient / WindowsServer Cook、Stage 和 UnrealPak CSV；再运行审计器，按实际CSV提交 --csv 与 --target Client/Server 及 --report 报告位置。
每个实际容器均需保存清单SHA-256、目标平台、唯一文件数、缺少的必需包及禁止包列表。
具体Cook计划见 Docs/Implementation/FaunaCookValidationPlan_20261011.md。

## 与实现进度对应
目前独立PIE报告 Saved/Validation/Fauna/IntegrationReview.json 显示10组GPU测试通过、鸟群与Fauna所有权检查通过、世界清理通过。通用群落行为17项原生测试通过。
Editor 定向模块构建已通过。后续完整 Client 构建 d706fa7b-0d26-47f3-98de-0d6475b75194 的 Build/result.json 为 Passed、退出 0；完整 Server 2dc35ef2-850a-4020-8b2c-0314f720fcc2 的底层 UBT.log 已显示 Result: Succeeded，但没有可读的外层汇总文件。准确范围及最终原生池测试证据见 FaunaDelivery_20261011.md，保留此前失败/中止历史。
正式新手村花点/水域锚点绑定、最终 Server Stage、目标硬件帧预算和人工美术验收仍需独立验证。下文分开记录定向 Cook、官方 Registry 和正式 Client Stage 的证据。

## 本次 WindowsClient 定向资源 Cook（后续实际执行）
- 实际目标：WindowsClient；范围仅为平台及项目环境Fauna两个内容目录；使用 NoGameAlwaysCook/NoDefaultMaps 控制根集，非正式整客户端发布。
- 执行器：UE5.8 UnrealEditor-Cmd Cook；日志为 Saved/Validation/Fauna/AssetCook_20261011T2256/Cook.log；进程退出码0、最终0 errors与0 warnings。
- 真实烘焙统计：Packages Cooked=568、Skipped by Platform=8、Total=576；Niagara GPU脚本编译计数=3。日志分别明确加载并编译 NS_DBA_ButterflyAzure、NS_DBA_FishSilver 与 NS_GP_Fauna_Base。
- 输出：Saved/Validation/Fauna/AssetCook_20261011T2256/Cooked/WindowsClient/DivineBeastsArena/AssetRegistry.bin 与 Metadata/DevelopmentAssetRegistry.bin 存在。
- 工程当前使用 Zen cooked output store（资源烘焙存储）；Zen manifest只有松散配置及元数据，不包含传统的全部.uasset文件。因此一次普通文件搜索为零不能证明Fauna遗漏；反过来也不能用上述成功计数证明全部20个必需资源都已打进最终Client发行容器。
- 后续已使用官方 DumpAssetRegistry 直接解析本次 Registry，无需重 Cook 或编写二进制字符串解析器；20 个完整包名均存在。该结果与最终 Stage 的 IoStore 容器审计分别记录，不能互相替代。

## 本次 Village WindowsServer 定向资源 Cook（后续实际执行）
- 实际目标：WindowsServer；配置为 VillageServer；指定正式地图 /DBAWorldPack_Village/Maps/L_Village_Start。
- 日志：Saved/Validation/Fauna/VillageServerAssetCook_20261011/Cook.log。进程退出码0、最终0 error/0 warning、耗时约6.30秒。
- 真实烘焙统计：Packages Cooked=644、Skipped by Platform=9、Total=653；含正式Village地图，但不等价于完整Server可执行文件构建或Stage完成。
- 两个Zen Cook运行包都已生成AssetRegistry.bin及DevelopmentAssetRegistry.bin。
- 辅助诊断 Saved/Validation/Fauna/CookRegistryNameProbe_20261011.json：从Cook后AssetRegistry.bin二进制内容查找20个预期资源名，WindowsClient识别20/20，WindowsServer识别0/20；保存两个原始Registry的SHA-256。此为名称存在性佐证，不是可靠的包级解析、Cook Store Oplog清单或最终IoStore容器审计。
- 后续官方 Registry 包名审计已确认两个 Fauna 运行根及开发目录均排除；最终 Server Stage 和新手村实际场景布置仍未由该 Cook 证明。

## 官方 Cook Registry 包名审计（实际通过）

官方 DumpAssetRegistry 已实际解析两份 Cook AssetRegistry.bin，报告为 Saved/Validation/Fauna/RegistryDump_20261011T152830Z/CookRegistryAudit.json，status=passed。Client 完整包数 568、资产对象 574，20/20 必需完整包名匹配，missingPackages=[]、forbiddenPackages=[]；Server 完整包数 644、资产对象 663，两个 Fauna 运行根及开发/ExternalActors/ExternalObjects 范围均为 0，正式 Village 地图存在。分页 Begin/End、声明包数、对象数和输入 SHA 均校验通过，报告 SHA-256 为 9b25bc501f14ca3dc5a6efb9d50f5e4f25e862c7ad7de4780eda07f34ea10488。

两次 Dump 进程均实际退出 0，各有 0 error、1 条既有 GamePlatformSpawnEligibility Python 暴露重名 warning；不能把这两份 Dump 日志记为零警告。该报告证明本次 Cook Registry 包名收录/排除，不证明可变 Zen oplog 的当前 payload，也不覆盖原始 FBX/PNG 等松散文件。

### 正式 FrontEndClient Cook/Stage 与主资源容器

既有正式入口实际运行 f0e7c867-1d0f-4d56-98a8-8462b2c20fac，Saved/Validation/FoundationM0/<RunId>/Cook-FrontEndClient/result.json 为 Passed、ExitCode=0。该运行保留真实 UI RHI、前端和鸟群门禁，生成独立 Stage 下的 Client.exe 与 IoStore；其实际 UAT 阶段 33.591 秒。

本次真实 UnrealPak 导出的 ContainerInspection/ClientContainer.csv 有 2761 行数据、1320 个唯一文件。使用项目原有 Tests/VFX/Fauna/ValidateFaunaContainer.py 执行后，20 个必需资源全齐，missing=[]、forbidden=[]，Fauna 文件共 26 个（包含运行包的附加数据）。报告位于 Saved/Validation/Fauna/FormalClientStage_f0e7c867/FaunaContainer.json。

共享机器的执行槽被其他常驻任务占用，因此取消了两个尚未启动的只读排队任务，改为读取完整 CSV 和同一审计器，在本地 Python 3.12 离线执行。重建副本后先校验原始字节 SHA-256：CSV=60509026ee1cb4db34eb0ef2270ceb839de826c9262710a7882d7f73fbcc37c8（885900 字节），审计器=05a2d79c6af73e9c1f0d6af8adf8ce8908a31390a3cec22bcb5b1e4b87c6745f（7922 字节），均与项目端完全一致。ExecutionReceipt.json 记录真实离线执行环境、退出 0 及原始远端路径；没有把这次 Python 检查称为 Windows 原生运行。

StageInventory.json 来自项目目录的完整只读分页，列出主 .utoc/.ucas/.pak 与 global.utoc/global.ucas。本次 Fauna 清单结论严格覆盖 DivineBeastsArena-WindowsClient 主资源容器；global 和 .pak 内部内容未另行展开为 Fauna CSV。它也不代替打包客户端内的蝴蝶/鱼群运行、美术验收或最终 Server Stage 检查。

# 资源归属整改设计（2026-10-31）

用户明确批准按ResourceDirectoryAudit20261031报告实施，通过Monolith迁移并保留现有效果；沿用原位Native执行，不再重复询问设计方向或执行方式。

## 目标与边界

在既有20个登记内容包内收敛资源归属、切断公共植物包对主工程和公共地形审核链对世界包的反向依赖。保留原生资源内容、资产叶子名、来源素材、材质图/参数、模型尺寸/几何、地图/PCG布局、世界/协议/主资产身份；不新建插件，不改玩法或美术。不通过文件管理器搬动uasset/umap。

## 选择与具体落点

- 选择同一Owner内职责目录迁移；不新建供应商插件或复制第二份资源，避免额外激活和版本真源。
- PN_OpenWorldFoliage中在用Meshes/Materials/Textures共51包，迁到DBAFoliagePack_Core/Vendor/PN_OpenWorldFoliage；其余40包样例迁到Game/Development/Vendor/PN_OpenWorldFoliage。
- 三份硬绑定Village贴图的审核地形母材质及九份实例共12包迁到DBAWorldPack_Village/Development/Terrain/Production10。当前仅开发审核使用，整体移Owner比重新编辑材质图更能保持效果；三个审核地图继续可用。公共十张V3色彩图迁到DBAPresentationPack_Core/Textures/Terrain/Production10，保持叶子名与RGB。
- 鸟群三个当前Flight网格及实际系统/定义保持当前稳定路径；七份旧网格原生迁到DBAPresentationPack_Core/Development/History/BirdFlock/Meshes，保留历史，不删源模型。
- StyleHex_Studio的55包样例迁到Game/Development/Vendor/StyleHex_Studio；保留内部引用、示例地图和来源。
- 175包清单是迁移唯一真源，先核对完整引用、目标冲突及脏包，再备份。任何已有目标、手工脏包、未登记Source变化或部分失败均停止该批并保留日志，不覆盖。

## 执行与保效

直接Monolith调用AssetTools原生RenameAssets并保存真实引用；代码、INI、当前作者/验证器/数据清单同步新路径，历史报告保留原事实。迁移产生的源路径仅在确认是UObjectRedirector、目标身份正确且原生/文本引用已收敛后原生清理，防止重定向继续维持反向依赖；不清除原来有用的兼容对象或未知资产。

保效证据包括迁移前后Texture原生导出像素、网格顶点/Bounds/材质槽、材质参数/引用、地图Actor布局，以及固定镜头截图；当前鸟群未迁移资产另做字节指纹。保存、独立进程冷读、源码回归、完整目标构建和实际Client/Server/演示Cook分别留证，不代填人工视觉或GPU通过。

默认禁止Development/Vendor、Village开发地形和鸟历史目录进入正式包；仅PluginDemoClient精确放开所需审核地形。专服排除在单一ServerPresentationExclusions真源登记并生成INI，避免第二套规则。

## 执行者裁定

原位使用已获授权的正式工程，因为UE正绑定此项目，复制插件或另建工程会破坏真实资产/引用验证；保留原有工作区改动，.git只读。必要时只受控重启本轮已核对的游戏实例，不清账号/角色。所有备份、导出、Cook、Stage、中间目录和日志均在工程Saved。

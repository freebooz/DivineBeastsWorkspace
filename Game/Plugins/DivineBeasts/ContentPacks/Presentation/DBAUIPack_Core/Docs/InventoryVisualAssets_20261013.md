# SSLM背包正式物品视觉资源制作清单（2026-10-13）

本清单为内容包真实资源制作合同，**不把尚未导入的PNG或纹理软路径声称为已完成的.uasset**。

项目公共第三层内容包：`DBAUIPack_Core`（SSLM UI公共内容包）。后端`inventory_item_policies_v1.json`（权威物品规则）只用于认证与数量；项目`DivineBeastsInventoryPresentationCatalog`（物品显示回退表）只提供中文名称、说明和未来纹理软路径。

首批8项：桃灵草（item.herb.peach）、艾叶（item.herb.mugwort）、湖心莲（item.material.lotus）、青岩矿石（item.material.stone）、桃木（item.material.peachwood）、草本药露（item.consumable.herbal_potion）、桃源村徽（item.quest.village_badge）、桑蚕丝（item.material.silkworm_silk）。

视觉制作标准：512×512、透明RGBA、3A东方神话道具绘画、形体饱满、小尺寸易辨认、柔和光照、真实材质、无文字和颗粒。每种图标必须通过Monolith真实导入、保存、重载、Widget实际显示和项目标准颜色链验收；当前Monolith端口不可用，**本轮资源文件状态为待制作**。既有背包页面`WBP_DBA_UI_Inventory`（背包面板）已经存在，不得用占位文件或从脚本虚构新的同名资产覆盖。

后续UI需由Monolith新增`SlotIcon_0..31`（32个物品格图标）和`QuickIcon_0..11`（12个快捷格图标），放在原控件树中并复核原有172控件与导航规则的兼容性。绑定后由C++事件快照触发异步软资源加载，不得逐帧加载图标、不占用服务器Cook。

明细以同目录`InventoryVisualAssets_20261013.json`（8类资源路径与生成状态）为准。

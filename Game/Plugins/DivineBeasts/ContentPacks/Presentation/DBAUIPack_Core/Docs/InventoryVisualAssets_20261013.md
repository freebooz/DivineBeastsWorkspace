# SSLM背包正式物品视觉资源制作清单（2026-10-13）

本清单原为源图和资产制作合同。**截至2026-10-14，Monolith已真实导入8张Texture2D（原生纹理）、创建8份主数据资产，并为原背包蓝图增加44个命名图标控件**。实际PIE/多客户端、Cook及人工高品质美术验收仍未完成。

项目公共第三层内容包：`DBAUIPack_Core`（SSLM UI公共内容包）。后端`inventory_item_policies_v1.json`（权威物品规则）只用于认证与数量；项目`DivineBeastsInventoryPresentationCatalog`（物品显示回退表）只提供中文名称、说明和未来纹理软路径。

首批8项：桃灵草（item.herb.peach）、艾叶（item.herb.mugwort）、湖心莲（item.material.lotus）、青岩矿石（item.material.stone）、桃木（item.material.peachwood）、草本药露（item.consumable.herbal_potion）、桃源村徽（item.quest.village_badge）、桑蚕丝（item.material.silkworm_silk）。

视觉制作标准：512×512、透明RGBA、3A东方神话道具绘画、形体饱满、小尺寸易辨认、柔和光照、真实材质、无文字和颗粒。8类图标均已通过Monolith真实导入、保存及纹理属性回读，分辨率512×512、sRGB=true、TEXTUREGROUP_UI，保留Alpha（透明通道）。它们来自概念素材裁切和放大，尚需更高精度的3A美术审查；实际Widget显示、正式Cook和游戏运行仍待验收。原背包蓝图`WBP_DBA_UI_Inventory`（背包面板）没有被替换，新图标控件已加在同一棵Widget树中。

Monolith已在原页面的`InventoryCanvas`（背包画布）新增`SlotIcon_0..31`（32个物品格图标）和`QuickIcon_0..11`（12个快捷格图标），均设为HitTestInvisible（不拦截按钮输入）。页面总计216控件，蓝图新编译0错误0警告，可访问性审查0问题；键盘和手柄导航仍需实际设备验证。C++采用事件驱动和平台异步软资源加载器，不逐帧查询图标。

本轮已在 `DivineBeastsInventoryScreen`（项目背包页面C++）加入对上述命名图标控件的事件驱动软加载及取消逻辑：仅真实控件存在才发起请求，按物品定义去重，失活取消并以加载代次屏蔽迟到回调。**本轮C++新增图标与数量布局代码仍待统一UE构建验证，真实蓝图和纹理资产已生成并回读。**

明细以同目录`InventoryVisualAssets_20261013.json`（8类资源路径与生成状态）为准。

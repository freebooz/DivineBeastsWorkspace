# ContentMapsAndAssets（内容、地图与资产）

项目Arena Runtime只引用逻辑MapId和Policy ID。

当前MainArena目录只有占位，没有真实.umap；因此Production MapId保持NotConfigured。禁止把Arena.Map.FoundationTest当正式地图，也禁止文本伪造.umap。

Asset Manager已扫描/DivineBeastsArena/Definitions下ArenaMode类型，但当前真实Production ArenaMode .uasset数量为0。

Content/Developer仅保留开发内容目录边界；任何Dev/Test规则必须明确命名并在Shipping Release Gate中排除。

Server未来只Cook gameplay geometry/collision/spawn/objective actors；纯视觉环境留Client Content Pack。

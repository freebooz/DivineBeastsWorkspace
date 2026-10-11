# 神兽联盟十种地表PBR逐款制作及人工签收台账

2026-10-11。每种地表独立制作BaseColor/Normal/ORM/Height，使用真实UE5.8导入、材质实例、可见预览、2×2平铺审查，最后由用户人工批准。未批准不得覆盖正式地图与既有稳定资源。

| 顺序 | 地表类别 | 技术源 | UE5.8真实资产 | 用户人工最终验收 |
|---|---|---|---|---|
| 01A | Rock01（林地风化岩备用） | 官方CC0扫描4×4K已验证 | 4纹理+1Material+2MI通过 | 用户指定保留备用；不作为裸崖主材质 |
| 01B | CliffRock01（青灰裸露悬崖岩） | 官方CC0 4×4K扫描及青灰BaseColor派生通过 | 4纹理+2Material+3MI原生回读通过，含三向世界投射版本 | 原图初审可用；真实世界投射预览异常暗黑，未通过最终视觉/人工签收 |
| 02 | Grass（青绿草地） | 待制作 | 待制作 | 待审核 |
| 03 | ForestSoil（森林腐殖土） | 待制作 | 待制作 | 待审核 |
| 04 | Dirt（普通泥土） | 待制作 | 待制作 | 待审核 |
| 05 | Scree（碎石坡） | 待制作 | 待制作 | 待审核 |
| 06 | Sand（湖岸沙地） | 待制作 | 待制作 | 待审核 |
| 07 | Gravel（河滩砾石） | 待制作 | 待制作 | 待审核 |
| 08 | RoadDirt（压实土路） | 待制作 | 待制作 | 待审核 |
| 09 | FarmSoil（耕作农田） | 待制作 | 待制作 | 待审核 |
| 10 | ShoreMud（湖岸湿泥） | 待制作 | 待制作 | 待审核 |

详细日志：Docs/Implementation/TerrainRockRealism01PBRManualReview_20261011.md（林地备用）与Docs/Implementation/TerrainCliffRock01PBRManualReview_20261011.md（新款青灰裸岩）。UE Shader与纹理技术通过不能替代真实AAA地形显示、人类签收或新手村Cook；世界对齐预览异常暗黑需修复。

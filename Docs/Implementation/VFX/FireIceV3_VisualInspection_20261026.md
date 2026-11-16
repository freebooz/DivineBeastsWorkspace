# 火／冰 V3 原生视觉检查（2026-10-26）

## 检查范围及证据

按用户最新要求检查已有火冰V3，而非将新P1—03/04草稿冒充既有版本。工程为DivineBeastsArena，UE5.8.0、D3D12真实PIE，1920×1080 SceneCapture原生PNG；没有后期增亮、滤色或生成图。角色镜头、曝光及飞行镜头分别固定为同一组参数。掌心参数采用实际默认0.5秒；飞行由隔离审核轨迹驱动，不是服务器弹道或按键联机验证。

四套原生系统均位于`/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/CastFlight/Niagara/`：NS_DBA_FirePalmCast_V3、NS_DBA_IcePalmCast_V3、NS_DBA_FireOrbFlight_V3、NS_DBA_IceCrystalFlight_V3。四套同步编译均为0错误、0警告；检查没有保存或更改这些源系统、材质、正式定义、服务器时序或技能结果。同步编译的内存派生缓存不等于源资产变更。

本次证据：`Saved/Validation/FireIceV3Inspection20261026/20261026T121255338970Z/Result.json`及同目录12张截图；检查驱动为`Tools/Unreal/VFX/DevelopmentMagic/CaptureCastFlightV3InspectionUE58.py`。

## 实际视觉结果

| 效果 | 截图 | 检查结果 |
|---|---|---|
| 火系双掌 | FirePalms_2.png，0.2888秒 | 两手定位正确，但火焰分离、呈条带状；缺少厚实掌心主体和稳定的小型高温核心。 |
| 冰系双掌 | IcePalms_2.png，0.2868秒 | 此固定曝光下几乎看不见，关键视觉未达标。并非没有运行：SimCache在后期记录ColdGather 12、ColdCore 3、CrystalSeed 8个粒子，位置与掌心一致，Alpha非零；仍需继续查材质/透明度/可读性链路，不能仅归咎于缺资源。 |
| 火球飞行 | FireFlight_3.png，0.7587秒 | 有立体主体和表面能量流动，但细丝感偏强，拖尾过细、层次不足。 |
| 冰晶飞行 | IceFlight_3.png，0.7590秒 | 实体晶核及尾迹可见，轮廓偏硬、石块感较重，外围寒雾不足。 |

结论：可用于人工查看当前真实状态，尚未达到视觉母版；编译通过不代表视觉通过。未执行多人联机、打包客户端、独占GPU耗时、透明过绘或最终人工批准。

## 与P1—03/04的区分

本次已有V3截图不作为P1—03/04完成证明。新审核资源保留在P1Hands目录，原生编译及保存已完成，但首轮运行暴露复制发射器未参与系统图、部分层缺失及造型粗糙问题。已对本次副本改用引擎完整AddEmitter路径重建，后续运行复验及视觉精修尚未完成；不得绑定或发布正式技能。

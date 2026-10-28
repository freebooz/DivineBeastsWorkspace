# 当前法术资源目录与命名收敛（2026-10-27）

依据六项同功能多实现审查整改。通过正式UE5.8/Monolith原位迁移42个包：9个当前法术阶段及33个实际传递依赖；另创建唯一显式Cook标签。保留稳定Definition路径和逻辑ID，没有复制第二套Niagara播放器。

实际规范目标包存在：42/42；原路径磁盘包仍存在：1。最终Monolith读回确认该包是ObjectRedirector，解析到规范NS_DBA_FireballCast，并非另一Niagara实现。其余原路径已由原生rename处理。原路径不是运行回退真源，最终发布由当前合同及禁止名单决定；未手工删除历史备份。

## 实际源与目标

以下均为DBAPresentationPack_Core挂载点内的包路径；省略共同前缀`/DBAPresentationPack_Core/VFX/`。

|原包路径|当前包路径|原生类型|
|---|---|---|
|`OrbProjectile/Review/V3/CastFlight/Materials/MI_DBA_FirePalm_Flame_V3`|`SpellProjectiles/Materials/MI_DBA_FirePalm_Flame`|MaterialInstanceConstant|
|`OrbProjectile/Review/V3/CastFlight/Materials/MI_DBA_IceBody_V3`|`SpellProjectiles/Materials/MI_DBA_IceBody`|MaterialInstanceConstant|
|`OrbProjectile/Review/V3/CastFlight/Materials/M_DBA_FireOrbVolume_V3`|`SpellProjectiles/Materials/M_DBA_FireOrbVolume`|Material|
|`OrbProjectile/Review/V3/CastFlight/Materials/M_DBA_FireTail_V3`|`SpellProjectiles/Materials/M_DBA_FireTail`|Material|
|`OrbProjectile/Review/V3/CastFlight/Materials/M_DBA_FrostTail_V3`|`SpellProjectiles/Materials/M_DBA_FrostTail`|Material|
|`OrbProjectile/Review/V3/CastFlight/Materials/M_DBA_PalmFireDirect_V3`|`SpellProjectiles/Materials/M_DBA_PalmFireDirect`|Material|
|`OrbProjectile/Review/V3/CastFlight/Meshes/SM_DBA_IceCrystal_V3`|`SpellProjectiles/Meshes/SM_DBA_IceCrystal`|StaticMesh|
|`OrbProjectile/Review/V3/CastFlight/Modules/NM_DBA_OrbBody_V3`|`SpellProjectiles/Niagara/NM_DBA_OrbBody`|NiagaraScript|
|`OrbProjectile/Review/V3/CastFlight/Modules/NM_DBA_TrailFade_V3`|`SpellProjectiles/Niagara/NM_DBA_TrailFade`|NiagaraScript|
|`OrbProjectile/Review/V3/CastFlight/Modules/NM_DBA_TrailLinkSpawn_V3`|`SpellProjectiles/Niagara/NM_DBA_TrailLinkSpawn`|NiagaraScript|
|`OrbProjectile/Review/V3/PalmRedesign/Materials/MI_DBA_PalmFire_Core_Review`|`SpellProjectiles/Materials/MI_DBA_PalmFire_Core`|MaterialInstanceConstant|
|`OrbProjectile/Review/V3/PalmRedesign/Materials/MI_DBA_PalmFire_Curl_Review`|`SpellProjectiles/Materials/MI_DBA_PalmFire_Curl`|MaterialInstanceConstant|
|`OrbProjectile/Review/V3/PalmRedesign/Materials/MI_DBA_PalmFire_Detail_Review`|`SpellProjectiles/Materials/MI_DBA_PalmFire_Detail`|MaterialInstanceConstant|
|`OrbProjectile/Review/V3/PalmRedesign/Materials/MI_DBA_PalmFire_Ripple_Review`|`SpellProjectiles/Materials/MI_DBA_PalmFire_Ripple`|MaterialInstanceConstant|
|`OrbProjectile/Review/V3/PalmRedesign/Materials/MI_DBA_PalmIce_Core_Review`|`SpellProjectiles/Materials/MI_DBA_PalmIce_Core`|MaterialInstanceConstant|
|`OrbProjectile/Review/V3/PalmRedesign/Materials/MI_DBA_PalmIce_Mist_Review`|`SpellProjectiles/Materials/MI_DBA_PalmIce_Mist`|MaterialInstanceConstant|
|`OrbProjectile/Review/V3/PalmRedesign/Materials/MI_DBA_PalmIce_Petal_Review`|`SpellProjectiles/Materials/MI_DBA_PalmIce_Petal`|MaterialInstanceConstant|
|`OrbProjectile/Review/V3/PalmRedesign/Materials/MI_DBA_PalmIce_Ripple_Review`|`SpellProjectiles/Materials/MI_DBA_PalmIce_Ripple`|MaterialInstanceConstant|
|`OrbProjectile/Review/V3/PalmRedesign/Materials/M_DBA_PalmAirRipple_Review`|`SpellProjectiles/Materials/M_DBA_PalmAirRipple`|Material|
|`OrbProjectile/Review/V3/PalmRedesign/Materials/M_DBA_PalmFireCurl_Review`|`SpellProjectiles/Materials/M_DBA_PalmFireCurl`|Material|
|`OrbProjectile/Review/V3/PalmRedesign/Materials/M_DBA_PalmIceCrystal_Review`|`SpellProjectiles/Materials/M_DBA_PalmIceCrystal`|Material|
|`OrbProjectile/Review/V3/PalmRedesign/Materials/M_DBA_PalmMist_Review`|`SpellProjectiles/Materials/M_DBA_PalmMist`|Material|
|`OrbProjectile/Review/V3/PalmRedesign/Meshes/SM_DBA_PalmFireCurl_A`|`SpellProjectiles/Meshes/SM_DBA_PalmFireCurl_A`|StaticMesh|
|`OrbProjectile/Review/V3/PalmRedesign/Meshes/SM_DBA_PalmFireCurl_B`|`SpellProjectiles/Meshes/SM_DBA_PalmFireCurl_B`|StaticMesh|
|`OrbProjectile/Review/V3/PalmRedesign/Meshes/SM_DBA_PalmFireCurl_C`|`SpellProjectiles/Meshes/SM_DBA_PalmFireCurl_C`|StaticMesh|
|`OrbProjectile/Review/V3/PalmRedesign/Meshes/SM_DBA_PalmIceCore`|`SpellProjectiles/Meshes/SM_DBA_PalmIceCore`|StaticMesh|
|`OrbProjectile/Review/V3/PalmRedesign/Meshes/SM_DBA_PalmIcePetal_A`|`SpellProjectiles/Meshes/SM_DBA_PalmIcePetal_A`|StaticMesh|
|`OrbProjectile/Review/V3/PalmRedesign/Meshes/SM_DBA_PalmIcePetal_B`|`SpellProjectiles/Meshes/SM_DBA_PalmIcePetal_B`|StaticMesh|
|`OrbProjectile/Review/V3/PalmRedesign/Meshes/SM_DBA_PalmIcePetal_C`|`SpellProjectiles/Meshes/SM_DBA_PalmIcePetal_C`|StaticMesh|
|`OrbProjectile/Review/V3/PalmRedesign/Niagara/NM_DBA_PalmConceptMotion_Review`|`SpellProjectiles/Niagara/NM_DBA_PalmConceptMotion`|NiagaraScript|
|`OrbProjectile/Review/V3/PalmRedesign/Textures/T_DBA_PalmIce_Normal`|`SpellProjectiles/Textures/T_DBA_PalmIce_Normal`|Texture2D|
|`OrbProjectile/Review/V3/PalmRedesign/Textures/T_DBA_PalmIce_Surface`|`SpellProjectiles/Textures/T_DBA_PalmIce_Surface`|Texture2D|
|`OrbProjectile/Review/V3/PalmRedesign/Textures/T_DBA_PalmMist_Flipbook`|`SpellProjectiles/Textures/T_DBA_PalmMist_Flipbook`|Texture2D|
|`OrbProjectile/Review/V3/PalmRedesign/Niagara/NS_DBA_FirePalmCast_V3_ConceptReview`|`SpellProjectiles/Niagara/NS_DBA_FireballCast`|NiagaraSystem|
|`OrbProjectile/Review/V3/CastFlight/Niagara/NS_DBA_FireOrbFlight_V3`|`SpellProjectiles/Niagara/NS_DBA_FireballFlight`|NiagaraSystem|
|`DevelopmentMagic/Niagara/NS_DBA_DevFireballImpact`|`SpellProjectiles/Niagara/NS_DBA_FireballImpact`|NiagaraSystem|
|`OrbProjectile/Review/V3/PalmRedesign/Niagara/NS_DBA_IcePalmCast_V3_ConceptReview`|`SpellProjectiles/Niagara/NS_DBA_FrostboltCast`|NiagaraSystem|
|`OrbProjectile/Review/V3/CastFlight/Niagara/NS_DBA_IceCrystalFlight_V3`|`SpellProjectiles/Niagara/NS_DBA_FrostboltFlight`|NiagaraSystem|
|`IceArrow/Niagara/NS_DBA_IceArrow_Impact`|`SpellProjectiles/Niagara/NS_DBA_FrostboltImpact`|NiagaraSystem|
|`DevelopmentMagic/Casting/Niagara/NS_DBA_DevArcaneMissilesCast`|`SpellProjectiles/Niagara/NS_DBA_ArcaneMissilesCast`|NiagaraSystem|
|`DevelopmentMagic/Niagara/NS_DBA_DevArcaneMissilesFlight`|`SpellProjectiles/Niagara/NS_DBA_ArcaneMissilesFlight`|NiagaraSystem|
|`DevelopmentMagic/Niagara/NS_DBA_DevArcaneMissilesImpact`|`SpellProjectiles/Niagara/NS_DBA_ArcaneMissilesImpact`|NiagaraSystem|

## 证据及保存边界

- `Saved/Validation/SingleImplementationConvergence20261027/SpellPresentation/NativeMigrationPlan.json`记录迁移及反向引用；`BackupIndex.json`逐原文件记录备份路径和SHA256。备份在工程Saved内采用扁平散列文件名，解决Windows路径长度限制；原始字节保留。
- 原生rename会在返回前保存。前置备份集合按AssetRegistry软/硬包反向引用预测，并拒绝本地化及外部包候选；运行时UPackage外部列表没有通过未验证API读取，后置dirty仅是残留检查。
- 九个Definition的ID、类、软引用、Cast schema和Bounds通过独立原生门禁；十五个当前/辅助Niagara系统经原生诊断为0错误、0警告，随后保存。
- FrontEndClient与VillageDevelopmentClient均在独立UE进程冷读通过，真实AssetManager均发现唯一`PrimaryAssetLabel:DA_DBA_SpellPresentationCook`。
- 原生创建Cook标签时修正UE5.8非BlueprintType规则结构的反射访问，采用Priority/bApplyRecursively/CookRule实际字段，不使用未导出的关键字构造器。
- 资源迁移没有修改技能GA。真实GAS首次31例30通过、1失败，来自旧测试把按键1当作定向飞弹；当前批准键位为1火球、2冰箭、3定向飞弹。十二个定向GA已原生读回正确，没有按错误假设重写内容；仅修测试输入并增加三键类路径严格断言后，最终31/31成功。既有GameplayCue搜索路径警告单独记录，未将其掩饰为零警告。
- 蓝图排列导致公共UI包哈希变化，发布所需真实RHI颜色门禁已重新执行通过。
- 最终GAS为31/31成功。实际Client Cook/Stage（092b4eeb）和Server Cook/Stage（34451cb9）均通过；同次15系统编译记录齐全、Shader诊断0、客户端退休系统0、服务器禁止条目0。首个客户端Cook因5个CPU系统缺同次编译日志未被放行，原记录保留；只在最终Cook显式启用引擎已有ForceDPCVars强制编译，没有削减门禁或删除缓存。
- 最终构建、包目录与exe哈希见`Saved/Validation/SingleImplementationConvergence20261027/DeliverySummary.json`。联网、听感和人工视觉本轮未重新执行；本记录不把原生诊断、资源存在或Cook通过等同GPU性能及人工验收。


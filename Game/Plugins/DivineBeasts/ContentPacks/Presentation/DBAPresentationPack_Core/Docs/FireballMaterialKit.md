# 标准火球术纹理与材质

用户参考图：`SourceArt/VFX/SpellProjectiles/Fireball/Reference_StandardFireball.png`。
本轮根据中间的大火球及右侧爆发制作可组合资源，已接入现有UE5.8 Niagara三阶段。图中的人物、城堡、文字、边框不进入纹理。资源属于项目公共表现包，不拥有伤害、飞行速度、命中或施法时序。

## 资源及使用位置

UE内容根为 `/DBAPresentationPack_Core/VFX/SpellProjectiles/`。纹理在 `Textures/Fireball/`，材质在 `Materials/Fireball/`。源图与提示词概要、SHA和分格合同在 `SourceArt/VFX/SpellProjectiles/Fireball/SourceManifest.json`。

| 资源 | 用途及合同 |
| --- | --- |
| `T_DBA_Fireball_CoreFlipbook` | 4×4、16帧循环火球表面/外围火焰，固定中心 |
| `T_DBA_Fireball_Trail` | 单张右侧宽亮根部、左侧渐细尾焰，适合拉伸Sprite或Ribbon |
| `T_DBA_Fireball_ImpactFlipbook` | 4×4、16帧一次性爆发，由点燃、膨胀到消散 |
| `M_DBA_Fireball_Flame` | 唯一新增火焰母材质：Unlit、Additive、双面、Sprite/Ribbon/Mesh用途 |
| `MI_DBA_Fireball_Core` | 16帧循环，24帧/秒，亮度3.2 |
| `MI_DBA_Fireball_Trail` | 1帧，亮度2.6，局部UV波动 |
| `MI_DBA_Fireball_Impact` | 16帧，26帧/秒，单次播放，亮度4.0 |
| `MI_DBA_Fireball_Ember` | 复用尾焰纹理的短小余烬，透明度0.65；由Niagara控制缩放和速度 |
| `MI_DBA_Fireball_Volume` | 复用现有 `M_DBA_FireOrbVolume`，三维主体半径参数25cm，Engine Sphere缩放0.5，主体直径50cm；不是新建体积执行器 |
| `MI_DBA_Fireball_AirRipple` | 复用现有 `M_DBA_PalmAirRipple`，局部空气扰动装饰 |

生成使用内置imagegen。三张原始PNG均为1254×1254 RGBA，原始字节完整保留；UE的 `ResizeToSpecificResolution` 在构建时缩到1024×1024，实际Texture2D尺寸已核对。此操作不会增加原稿细节，也不会用填边破坏4×4分格。灰度作为线性强度数据：关闭sRGB、MASKS压缩、保留Alpha、Clamp。翻页素材不生成Mip，并在材质中限制本格UV及淡出边界，避免小Mip和采样越界混入相邻帧；此选择仍需结合实际屏幕尺寸和GPU采样实测审核。

火焰颜色由材质参数提供。外焰默认引用现行色彩规范的屏幕sRGB `F16B46`；中焰 `FFA31A` 和高温核心 `FFF3C4` 为本参考图的内容选择，均先按sRGB标准转换到线性再写入。颜色参数、亮度与透明度分别可调。RGB不预乘Alpha，Additive路径只应用一次透明度；启用12cm深度交界柔化。

## Niagara组合合同

1. 既有 `NS_DBA_FireballCast/Flight/Impact` 仍是唯一阶段系统。本轮已原位接入新实例及唯一NM_DBA_FireballMaterialParameters，Emitter数量4/4/4；没有创建第二套System、Definition或Catalog。稳定绑定身份不变。
2. 主体优先使用受控三维Mesh与 `MI_DBA_Fireball_Volume`，外围火舌复用Core实例。不要把多个交叉大卡片当作三维主体；体积材质本身是既有实现，成本必须实测。
3. Sprite或Ribbon Renderer的 **Sub Image Size设为1×1**。4×4分格已由材质执行，不可再由Renderer重复分格。帧顺序从左至右、从上至下，材质自动在相邻两帧间混合。Texture的U轴为原稿左右方向；Sprite旋转或Ribbon朝向由Niagara控制。
4. Dynamic Material Parameter索引0的四通道依次是：R=`Particles.Age`，单位秒；G=帧偏移，默认0；B=消散进度0—1，默认0；A=亮度/透明强度倍率，**必须设为1才正常显示**。Niagara的动态参数绑定须显式提供四个值，不能让未绑定通道默认0。母材质静态预览默认值为(0,0,0,1)。
5. `ParticleColor` RGB作为乘色、A作为生命周期淡入淡出。默认白色乘色，A=1。`ManualFrame=-1`表示自动播放，0—15用于单帧审核；正式实例已恢复-1。`UseNiagaraAge=1`，每个粒子有独立时间；一次性命中不应使用全局时间。Core循环；Impact不循环，建议粒子有效期约16/26=0.615秒并于结束清理。
6. 尾焰用单张实例，长度、速度、旋转与局部拖尾由现有Niagara控制，不改变服务器真实投射物。命中只能消费服务器已确认事实，不能由视觉接触自行触发伤害。
7. 空气扰动只绑定到法术附近的有界Mesh，跟随施法、飞行和命中所属阶段启停，不能全屏扭曲。同一实例`P_Intensity=.008/P_Opacity=.06`，由Niagara ParticleColor Alpha按Epic/High/Medium/Low的1.5/1/.5/0倍调节。Low另由原生Renderer质量掩码30关闭。飞行范围为主体1.15/1.10/1.05倍；命中按粒子Age膨胀并在0.7秒内淡出。这些是制作参数，未宣称GPU预算通过。Low仍保留火球和关键命中反馈。

## 重复执行与验收边界

作者入口 `Tools/Unreal/VFX/SpellProjectiles/AuthorFireballMaterialsUE58.py`，只能通过Monolith `editor.run_python` 在本轮拥有的锁定UE5.8编辑器执行。拒绝其它任务同名目标；只对新建并登记的10项资产重做，不运行旧Review作者。逐包保存，不全局保存其它任务脏包。

原生证据位于 `Saved/Validation/FireballMaterials20261031/`：保存/重载清单、材质编译统计、固定相机实际UE截图和逐帧预览。预览图是素材显示检查；它不证明完整法术系统、Cook、打包、联机、声音、多人GPU开销或人工艺术验收已完成。

锁定引擎的材质实例Scalar/Texture setter实现始终返回false，即使实际写入成功。因此工具同时检查参数存在性及写后值/纹理回读，不采用该错误返回值作为验收。初次平面网格的黑图是默认相机沿平面边缘观察；球体预览及俯视平面相机提供反证，未为此改写正确的RGBA动态参数连接。

## 当前原生接入与回归

原位接入作者为`Tools/Unreal/VFX/SpellProjectiles/IntegrateFireballNiagaraMonolith.py`。适配HLSL唯一源为同目录`FireballMaterialParameters.hlsl`，三阶段共用一个模块；新实例共用一个火焰母材质。原Cast/OrbBody共享运动模块未改写，冰系不受火球尺寸规则影响。

飞行保留CPU世界空间Ribbon，寿命0.22秒、每秒100个点、初始宽24cm；原生SimCache在4000cm/s运动夹具下得到22点、813.74cm轨迹，四档均保留50cm核心与长尾。此项证明表现历史轨迹，不代替服务器运动或联机验收。球头及尾焰为自发光材质，局部热扰动为独立有界装饰。

Monolith duplicate_emitter缺少系统执行图重建，曾导致新增扰动Emitter编译正常而零粒子；已改用add_emitter的引擎完整路径修复，三个系统均有8个系统Emitter节点。验证器检查此连接以及真实粒子，不以编译通过代替播放。Native/Cook/运行证据分别保存在`Saved/Validation/FireballNiagara20261031/`。

独立审查发现原空气材质Opacity直连常量。现有共享母材质增加`P_UseParticleAlpha`默认0，保持全部旧实例输入不变；火球AirRipple唯一实例显式设1，使Opacity=P_Opacity×ParticleColor.A，折射仍按原通道控制。前摇/命中淡出与Low同时收敛透明度，避免暗壳残留。修改前原包已备份；未建立第二个扰动母材质。

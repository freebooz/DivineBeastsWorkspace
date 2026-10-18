# SSLM 定向飞弹V1拆解与完整Niagara制作提示词

用户本轮明确授权制作第二项攻击「定向飞弹」，不将此授权解释为凝锋已通过人工验收，也不扩展其余模板。当前为编辑器原生审核候选，视觉一致性、正式技能/权威投射物接入及打包GPU/Cook待验收。

颜色继续遵循[全局配色V3.0](../../Art/SSLM全局美术配色策略与色彩设计规范_V3.0.md)中技能主体色、局部能量高光和威胁可读性的分工；本次橙金来自用户指定参考，不新增五行玩法或竞争色板。所有截图保留引擎原生像素，不后期加光、锐化或用生成图代替实际材质。

## 参考图拆解

参考原图1106×669。施法手部约(270,416)，弹头尖端约(982,153)，梭形主体从约(680,260)逐渐收敛到尖端。原图为双拖尾；按用户最新要求改为三条加粗拖尾，相位0°、60°、120°，相邻相差60°。中心金白高亮只占局部，外壳保留橙色暗面、尖棱与空隙；不是火球、光束或整段等宽圆管。

原图未提供相机元数据。本轮采用参考构图校准的1656×1002、水平FOV45°斜俯视镜头。训练人偶、石板和环纹只作空间/比例代理，不声称复刻原场景角色、动作与园林美术。

| 元素 | 纹理/材质方案 | 原生结构 |
| --- | --- | --- |
| 金白能量核/尖端 | `T_DBA_MissileV1_Head_Color`；`M_DBA_MissileV1_Core`，Unlit半透明，暖金到象牙金，局部高亮、端部淡出 | 有体积的细长内核，朝局部+X |
| 橙金三瓣梭形弹壳 | 同一弹头纹理的独立UV；`M_DBA_MissileV1_Petals`，保留橙色暗面、细流与边缘渐隐 | 三条可编辑Bezier包覆瓣，螺旋角错开120°，真实透镜截面 |
| 三条历史拖尾 | 独立`T_DBA_MissileV1_Trail_Color`；`M_DBA_MissileV1_Trail`，Additive、解析连续Alpha与柔边，粒子色区分金/橙/浅金 | 三个世界空间Ribbon Emitter，相位0°/60°/120°；出生位置保留为历史位置，不在Update把旧点拉回弹头 |
| 空气波动 | `M_DBA_MissileV1_AirWave`，低频解析波、RM_2D_OFFSET，强度初值0.35 | 同一弹头网格的有界空气包络，共用粒子生命周期，不增System |

弹头/拖尾源图分别由内置image_gen生成，保存原始RGB/Alpha与完整英文提示词。它们只作材质输入，不能将生成图当原生效果截图。颜色sRGB、BC7、带Mip、最大2048，解析遮罩与折射波在材质中线性计算；不对原稿做Gamma补偿、锐化或截图后期。原始图像Alpha不再切碎主体或尾迹；Head纹理只贡献3%低对比度RGB变化，Trail只贡献1%。轮廓由真实几何与连续解析遮罩控制，避免颗粒边和碎亮线。

## 完整Niagara制作提示词

以下为可交给Niagara制作人员/工具的完整约束。实际资产由Monolith按同目录作者配方创建，提示词本身不等于资产已经存在。

> 在锁定UE5.8主工程中，为SSLM公共表现包制作一个东方神话橙金定向飞弹审核候选。资源仅归第三层`/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V1/DirectedMissile`，平台使用既有Projectile Definition和GamePlatformVFX唯一执行器，不创建玩家、伤害、碰撞、服务器运动、追踪Actor、第二套播放器/池或Data租约。
>
> 视觉参考本包`SourceArt/VFX/CommonSkillVFX/V1/DirectedMissile/Reference_DirectedMissile.png`，同时执行最新的三条加粗尾迹要求。弹头是有厚度的细长梭形，局部+X为尖端，三片橙金能量瓣向金白尖端汇聚，前后层叠、保留橙色暗面与细窄空隙；细腻能量沿表面向前流动，禁止做成圆球、均匀实管或大片白色过曝轮廓。三条金色/橙色/浅金拖尾沿实际经过位置飘动，逐渐变细、变淡，不把整个飞行路径烘焙成贴图，不把图中人物/地面导入为特效。
>
> 创建唯一`NS_DBA_DirectedMissile_V1`，四个CPU Emitter：`MissileHead`本地空间，一次Burst1粒子，三材质Mesh Renderer；`GoldTrail`、`AmberTrail`、`SoftGoldTrail`世界空间，各SpawnRate96/秒，有限Once发射窗口0.76秒，粒子寿命默认1秒。Mesh用`SM_DBA_DirectedMissile_V1`，槽位Core/Petals/AirWave；三条Ribbon共用Trail材质，通过粒子色和初相0/π÷3/2π÷3区别。HeadLifetime默认0.85秒，HeadScale1，TrailWidth26厘米，Intensity1，User.Direction默认(1,0,0)。审核飞行0.75秒，之后尾迹有限消退，完整审核采样2秒。用户指出上一版加粗后失去比例，本次保持弹头长度，外包覆截面理论最大宽度保持约0.19L，内核直径增加为0.112L，瓣厚增加至0.038L，填补内部线笼空洞而不再次放大整体轮廓；L为弹头轴长475.71厘米。拖尾宽度由20厘米升为26厘米，末端随年龄渐细。
>
> Head Update只设置本地位置0、受限缩放与颜色，实际头部位置来自所属视觉组件的当前变换。Tail Spawn读取`Engine.Owner.Position`，按User.Direction建立横向基底，加入50厘米正弦横摆和22厘米竖向偏移；初相不同，Clock取Emitter.Age。零方向和接近竖直方向使用明确正交回退，避免Normalize零向量。使用稳定UniqueID作RibbonLinkOrder。Tail Update保留既有Position，只按Age/Lifetime缩小RibbonWidth并衰减Alpha。Ribbon Screen Facing，有限四级细分，按世界长度180厘米平铺UV；禁止每帧把历史拖尾点重新定位到当前弹头。
>
> Core/Petals表面分别使用Head纹理；Trail使用独立Trail纹理；持续视觉动画由材质ParticleRelativeTime和Niagara原生更新执行，不轮询Gameplay。纹理Mip偏移初值+1.8，柔化解析边缘，不使用高频噪点或全局Bloom掩盖粗轮廓。AirWave使用有界包络及原生2D折射偏移，低频波动，随所属粒子的Age启动/结束；不得全屏扭曲或遮蔽敌方危险信息。记录折射、SceneColor和额外材质段成本，不能宣称免费。
>
> 绑定独立审核CombatProjectile Effect Type；有限FixedBounds暂定相对组件[-2500,-2500,-1200]—[2500,2500,1200]厘米，后续正式射程/转向/质量合同需实测修订。设置AutoRelease可用、固定Bounds要求、最大审核寿命3秒；不将待批准实例数/GPU预算写成已达标。所有材质、Niagara、Definition、地图通过Monolith编译、保存并独立冷加载；报告Emitter/粒子/材质及真实验证结果。
>
> 审核相机按ReferenceGuide固定，曝光Manual Bias0、物理曝光关闭、MotionBlur0、Bloom0.18。输出同角度原生PNG和连续原速/慢放视频，保留原始帧与哈希。首先核对弹头轮廓/暗面/细锋、三尾相位/渐细、尾迹真实历史位置和空气波动；人工未通过不得宣称与参考一致或Production Ready。

## 半透明与发光分层修订

用户补充询问透明/发光后，当前修订明确同时使用两种机制。Blend Mode负责透出后方瓣面，Emissive负责自身亮度，两者并不互斥。Core/Petals采用Unlit+Translucent：内核Alpha0.80维持主体；薄瓣Alpha随视角和外缘在0.30—0.86变化，保留橙金暗面。Trail使用Additive，主要表现连续流动亮带，不把整个弹头改为叠加而丢失暗部。

仅将局部瓣边/接缝高光目标从线性(4,2,0.35)微调至(4.6,2.5,0.48)，保留Core/Petals/Trail发光倍率2.3/1.75/3.0与Bloom0.18。尖端辉光共用Core槽位、局部更亮；没有新增粒子灯光、全屏泛光或独立System。半透明提高层次但仍有Overdraw成本，不能把材质类型本身当成高品质或性能达标的证明。

## 权威与接入边界

锁定目标的实际弹道仍由GamePlatformCombat服务器逐运动子步追踪施法时原目标当前碰撞包围盒中心；后续切换选择不能改写已发射目标，无目标保持既有直线，不扫描换目标。视觉组件消费已经确认的运动变换；确认命中VFX与SFX独立去重，资源失败不影响权威结果。

本轮只制作Projectile审核Definition和原生表现。编辑器有限运动工具用于相机/动效取证，不是生产追踪器；没有将候选接入十二生肖、正式Catalog或主资产扫描，不将编辑器直线演示称为移动目标联机验证。

## 本轮比例与空气质量合同

本轮修订 `translucent_energy_layers_03`：轴长约475.71厘米，最大外截面半径约0.095L。内核半径0.056L，三瓣径向半宽0.047L、切向厚度半轴0.019L，瓣中心距轴0.048L；相比退回版填补空心、增加厚度，保持外形比例。尖端小核半径0.012L，另以同一Core槽位承载局部辉光和两道聚能细芒，不增加材质段。聚能细芒仅表示飞行尖端，不能代替服务器确认命中。

三条尾迹最大RibbonWidth为26厘米，初相0、1.047198、2.094395弧度（0°/60°/120°），同一16rad/s摆动，相邻差60°。空间横向/竖向振幅50/22厘米；前0.10秒平滑展开，0.56—0.76秒平滑汇聚到头部，避免悬空短线头。出生点柔化遮罩保存在粒子Color.B，Update保留遮罩并衰减宽度/Alpha，消除尾端平切口；材质只使用Color.G区分三尾暖色。以上时间窗口属于当前有限审核Definition，正式更长飞行应按服务器表现合同配置，不能宣称已覆盖持续追踪生命周期。

源检查增加有向体积验收：Core、Petals、AirWave均须朝外，不能用双面材质掩盖倒面。独立审查发现近侧壳曾被剔除，已在本轮修正轴向网格面序并重开保存源检查；尖端球面保留本来的正确方向。

空气四档拟定Epic/High/Medium/Low强度0.35/0.25/0.12/0，范围为同一局部包络的100%/85%/65%/关闭；当前原生候选只验证默认0.35，尚未把四档自动映射或多人GPU成本标记完成。关闭空气只影响装饰，不移除弹头和三尾方向信息。正式档位参数与范围变体须在人工通过后经GamePlatformVFX既有质量机制接入，不建立新预算真源。

## 可复现的材质与模块细节

四个专用材质共用两张纹理；Mesh有Core/Petals/AirWave三材质段，三条Ribbon共用Trail。Core/Petals使用`frac(UV.x-RelativeAge*0.12)`连续采样，纵向范围0.16—0.84，Mip偏移1.8；其流动依靠低频解析波而非高频颗粒。Core发光倍率2.3、Petals为1.75、Trail为3.0，审核曝光与Bloom0.18保持固定。内核与瓣面保留暗橙主体，只有外缘、弯曲叠层接缝和尖端聚能局部高亮，避免均匀粗亮中轴。

瓣面使用连续坐标构造弯曲羽瓣分区，先对未折返坐标求`fwidth`再取`frac`，避免折返导数放大造成断裂白边。外围金边只提高外侧UV，不同时把内外缘均提亮为直管。VertexColor.R区分瓣面层与尖端辉光，FBX导入要求Replace顶点色；两道细芒与辉光共用Core槽位。

```hlsl
// 三Emitter输入Phase=0、1.047198、2.094395。只在Spawn确定世界历史点。
float3 d = length(Direction) > 0.001 ? normalize(Direction) : float3(1,0,0);
float3 axis = abs(d.z) < 0.95 ? float3(0,0,1) : float3(1,0,0);
float3 side = normalize(cross(axis,d));
float q = Clock * 16 + Phase;
// VectorVM不支持本模块的smoothstep，使用等价三次多项式。
float begin = saturate(Clock / 0.10);
float end = saturate((Clock - 0.56) / 0.20);
float birthFade = begin * begin * (3 - 2 * begin);
float attach = birthFade * (1 - end * end * (3 - 2 * end));
Particles.Position = Owner + (side*sin(q)*50 + float3(0,0,cos(q)*22))*attach;
Particles.RibbonWidth = Width;
Particles.RibbonLinkOrder = Particles.UniqueID;
Particles.Color = float4(1, Phase < 0.5 ? 0.65 : (Phase < 1.5 ? 0.32 : 0.48), birthFade, 1);
// Update不修改Position；B通道生命周期内保留出生柔化遮罩。
float t = saturate(Age / max(Life,0.01));
Particles.RibbonWidth = Width*(1-0.65*t)*(0.15+0.85*Particles.Color.b);
Particles.Color.a = pow(1-t,0.65)*Particles.Color.b;
```

Trail横截面由`exp2(-v*v*70)`连续宽带和`exp2(-v*v*1200)`细亮心叠合，橙金渐变、柔边和有限出生渐隐；不使用源图Alpha侵蚀成碎线。Core主体Alpha0.80；Petals随视角与外缘在0.30—0.86之间变化，两者受ParticleRelativeTime有限启停包络约束；尖端辉光依视角柔化。空气二维偏移使用低频正弦组合乘`AirWaveStrength*0.003`及有限UV/年龄包络；不是全屏后处理。精确配方见`AuthorDirectedMissileV1UE.py`，两份原始纹理提示词继续随源文件保留。

## 文件与验收记录

- 源：内容包`SourceArt/VFX/CommonSkillVFX/V1/DirectedMissile`中的参考、两张RGBA纹理、`HeadTexturePrompt.txt`、`TrailTexturePrompt.txt`、可编辑`.blend`、FBX与ReferenceGuide。
- 工具：`Tools/Unreal/VFX/CommonSkillVFX/{Build,Author,Capture}DirectedMissileV1*`；独立保存源检查`Tests/Assets/ValidateDirectedMissileV1Blend.py`。
- 证据：`Saved/Validation/CommonSkillVFX/DirectedMissileV1`，以最终Verification及实际原生媒体为准。尚未完成的项目必须逐项标记，不用文件存在或提示词替代原生/性能/人工验收。

本轮相对用户退回版，保持外包覆比例，补实内部、加厚曲面、改为弯曲羽瓣接缝，尖端局部聚能，三尾连续且起点渐隐。当前仍有与概念图不同的细节：原生瓣面偏规则，参考图的自由飘逸火焰层次尚需人工审核；训练场代理也不是原图完整场景。不能据本轮技术验收声称画面完全一致。

本轮实际结果：17份原生保存包；一个Niagara System、四CPU Emitter；11168三角形、6097顶点、Mesh三材质段，三尾共用额外一个Trail材质。独立UE5.8冷加载根进程PID41064、退出码0，Niagara编译0错误/0警告，保存包SHA在冷验证前后及媒体发布时一致。十实例检查在35步时分别为弹头1粒子/三尾各54粒子，三尾各保留约662.50厘米历史跨度；五实例Deactivate后与其余自然结束实例在2.5秒时均不再活跃，临时Actor清理完成。这些是编辑器功能检查，不是GPU或联机性能测量。

真实材质编译通过；Monolith统计Core/Petals/Trail/AirWave像素着色指令分别210/231/161/146，报告保留在`MaterialCompileStats.json`，这些编译统计不是GPU耗时。相较退回版网格10528三角形增加640至11168，仍为三材质段，System/Emitter数量不变；当前存在半透明及空气折射成本，必须后续做多人Overdraw/GPU测量。

最终原生序列为`RenderRuns/20261018T131417986824Z`，1656×1002、60帧，固定参考镜头。`DirectedMissileV1_Native.png`直接复制Frame_021（0.733秒），没有截图后期；同目录`DirectedMissileV1_Native.mp4`为30fps/2秒原速，`DirectedMissileV1_3xSlow.mp4`为10fps/6秒慢放。全部视频解码帧与原生序列在编码输入YUV420p空间核对，无损WebP另外逐帧核对RGB与时间线。视频帧率为采样/编码帧率，不代表实测游戏FPS。

空气开关对照保留关闭/关闭/开启三个原生采样，默认强度0.35；关闭对照逐像素相同，开启产生3399个不同像素，差异见`AirWaveVerification.json`。原材质已恢复并保存，当前17包通过冷加载。纹理源保持2172×724 RGBA，真实UE显示Mip为2048×512、sRGB/BC7；来源、提示词及模型/原生包SHA见`TextureManifest.json`与`Verification.json`。

仍待人工确认弹头/尾迹粗细、三条相位观感、细节与参考图的一致性。正式运动目标/技能接入、四档质量、Shader Complexity/Quad Overdraw、多人GPU与客户端/服务器Cook均未验收。本轮不据静态帧或编译结果宣称可投产。

## 后续授权的运行绑定（2026-10-18）

用户随后明确要求“此技能绑定到角色按钮1，原治疗绑定按键5并能加血”。已建立独立`VFX/CommonSkillVFX/Runtime/DirectedMissile`运行闭包，保持历史审核资产不变；沿用权威Fireball单发行为，以独立能力身份/冷却和DBA.Development.DirectedMissile语义接入唯一平台表现链。十二生肖按钮/数字1授予新飞弹，原Primary治疗身份移至按钮/数字5，2火焰弹/3寒冰箭/4奥术保留。

运行版弹头生命周期2.2秒、三条世界历史Ribbon覆盖实际飞行，朝向来自Engine.Owner.SystemXAxis并由表现上下文接收权威弹体朝向；取消沿用既有实例清理。相邻相位60度、宽度26厘米，局部空气包络和半透明/局部发光材质继续使用当前设计。

独立冷重载、完整Editor及Client/Server构建、真实平台输入与服务器回血测试通过；治疗50%→70%、最大生命限制、10秒冷却、沉默时不回血/不进入冷却均已验证。真实Client/Server容器核对通过，10项运行VFX包随Client发布，三项作者Module正常剥离，Server无项目VFX泄漏。原生运行资产飞行预览及明细归`Saved/Validation/CommonSkillVFX/DirectedMissileSlotBinding`和[执行记录](DirectedMissileSlotBindingPlan_20261018.md)。人工视觉、四档/多人GPU、真实Village/MainArena网络准入仍待验证；不将本次接线授权视为视觉签收。

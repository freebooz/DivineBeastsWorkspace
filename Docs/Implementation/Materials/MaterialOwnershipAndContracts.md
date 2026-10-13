# 通用材质函数库：三层所有权与接口合同

## 一、正式归属

- `GamePlatformSurface`（平台表面材质插件）：环境状态、七个既有稳定材质函数、两套表面母版、全局MPC（材质参数集合）、无关世界的通用覆盖/地形/植被/建筑着色函数，以及SurfaceMark（表面痕迹）已有贴花服务。
- `GamePlatformVFX`（平台视觉特效插件）：Niagara（粒子特效）专用纹理流动、软粒子、序列帧、发光、溶解、扭曲等函数/材质；运动、发射和生命周期仍由Niagara负责。
- `GamePlatformUI`（平台界面插件）：User Interface（界面渲染域）函数和中性母版；仍以UI.Style.*（统一界面样式语义）与ThemeDefinition（主题数据定义）作为唯一主题机制。
- `GamePlatformWeather`（平台天气插件）：服务端天气事实与客户端可视数值；仅由客户端桥接Surface状态，不拥有表面着色器，不与Surface构成反向依赖。
- `GamePlatformPCG`（平台程序化内容生成插件）：分布规则、生态区域、实例选择，不复制Shader（着色器）函数，不以材质遮罩代替玩法权威数据。
- `GamePlatformPresentation`（平台表现语义插件）：中立特效请求和已有执行边界，不复制Surface/VFX/UI函数。
- `MobaPresentation`（MOBA表现语义插件）：只有出现真实跨MOBA复用场景才组合竞技范围圈、阵营警示和施法读条，消费平台功能，不建立平行材质函数库。
- `DBAWorldPack_*`（神兽联盟世界内容包）、`DBAPresentationPack_Core`（公共表现内容包）、`DBAUIPack_Core`（界面内容包）：项目专属纹理、材质实例、技能外观与UI主题；不将项目纹理/特效放入平台插件。

## 二、依赖与权威不变量

1. 依赖仅允许 `DivineBeasts → MobaCommon → GamePlatform`（项目→MOBA→平台）或项目直接依赖平台；不得反向引用项目名称、路径、资源或C++类型。
2. `GamePlatformSurface`不得依赖Weather、PCG、VFX、UI、MobaPresentation或DBA；`GamePlatformUI`与`GamePlatformVFX`亦不得为了复用少量数值运算而在运行时引入Surface。
3. 材质是客户端视觉计算，不能决定服务器降雪摩擦、技能结算、导航碰撞、真实水深或损坏判定。
4. 若同一算法跨插件稳定复用，先考虑UE原生表达式/现有函数；确需提取公共内容资产时，应另立兼容/依赖评审，不能擅自新建`GamePlatformMaterials`（平行通用材质插件）。
5. 保存现有 `/GamePlatformSurface/MaterialFunctions/Masks/MF_GP_SlopeMask`（坡度遮罩）等七个函数，以及两个母版和MPC全部路径、参数名称及含义。更名、参数破坏和移动必须进行引用者清单、逐实例迁移与可回退验证。
6. 标量遮罩默认范围0~1；积雪高度为厘米；温度为摄氏度；颜色纹理sRGB（颜色空间）与ORM（遮蔽/粗糙度/金属度）数据纹理采样颜色空间区别明确；法线贴图须使用匹配压缩和法线约定。
7. 公共函数入参遵循基础表面属性→局部可选遮罩→环境全局值→覆盖材质属性，能使用Material Attributes（完整材质属性）的母版必须处理法线/粗糙度，避免只改变BaseColor（基础色）。
8. 每个材质母版的Material Domain（材质域）、Blend Mode（混合模式）、Shading Model（着色模型）按领域分别选择；UI不得直接复用表面PBR母版，透明VFX不得因方便强行使用不透明母版。
9. MPC必须仅保留已有八个全局参数；对象局部受击/沾水/结冰用材质实例或对象级参数；跨世界更新按World（世界）作用域隔离并事件驱动。
10. 服务器资源裁剪必须以Server Cook/Stage（服务器烘焙/暂存包）实际产物为准，非声明ClientOnly（仅客户端）就视为通过。

## 三、路径身份与版本策略

新增路径暂为`ProposedAssetPath`（建议资产路径）；只有真实UE编辑器资产创建、Shader编译、保存重开、消费者引用通过后才能提升为稳定合同。原有`ContractAssetPath`（冻结路径）绝不因对齐新命名直接迁移。

`MaterialCapabilityRegistry_V1.json`（201项能力机器索引）为能力记录唯一真源；`MaterialFunctionLibraryIndex.md`（中文索引）仅为审查视图。

# 自然飘落物视觉特效系统分步执行计划（2026-10-11）

## 任务边界
- 目标：为《神兽联盟》统一制作桃花、枫叶、竹叶、银杏叶的持续飘落、落地、静置、风吹滑动、翻滚、打旋及再扬起效果；角色选择／创建共同使用同一预览工作室中的桃花预设。
- 工程：DivineBeastsWorkspace/Game/DivineBeastsArena.uproject（正式主工程），UE5.8；所有实现增量进行，不覆盖现有未提交界面修改。
- 三层依赖：DivineBeasts（第三层）→（按需跳过MOBA）→GamePlatform（第一层）。MobaCommon（第二层）本任务不新增逻辑。
- 固定插件：GamePlatformVFX、GamePlatformPresentation、GamePlatformData；可选接入 GamePlatformWeather、GamePlatformPCG；项目组合使用 DBAClient/DBAFrontEndPack/按现行规范注册的项目公共表现内容包。
- 原则：1套通用Niagara运动发射器＋按种类替换纹理/材质/渲染器预设＋项目Definition；不新增专用“落叶插件”、第二套资产管理器或VFX WorldSubsystem。
- 素材版权：本任务程序生成的源纹理，无第三方转用限制；材质/蓝图/uasset/umap必须由真实UE生成与保存，不得用扩展名伪造。

## 阶段与完成标准
| 顺序 | 阶段 | 具体内容 | 可验证交付 |
| --- | --- | --- | --- |
| P0 | 基线与合同 | 保存工作区状态、确定目标路径/接口/引擎、检查工具；不修改现有用户资源 | 本计划及环境清单 |
| P1 | 先制作纹理源资源 | 4类落叶和花瓣（桃花/枫叶/竹叶/银杏），各生成透明Albedo与法线贴图；固定种子、分辨率、哈希；预设原始材质和参数 | SourceArt中的真实PNG、素材manifest与视觉预览图 |
| P2 | 真正导入贴图/材质 | 通过锁定UE5.8编辑器导入Texture2D，设置sRGB/法线压缩/透明；创建真实材质母版与4种实例、保存并重载 | 可读回Texture2D/Material/MI .uasset |
| P3 | 通用Niagara及地面逻辑 | 共享运动：生成/重力/阻力/摇摆/风场；落地→静置→贴地滑动→阵风扬起→再飘落（限定循环/生命周期）；单平面参考高度和法线，复杂地面后续分级 | 真实Niagara System与模块、蓝图作者脚本/原理合同 |
| P4 | Definition和注册 | 定义参数白名单、四个项目变体ID；用GamePlatformData租约及GamePlatformPresentation发布目录，不绕过已有服务。建立加载缺失回退 | 真实Definition .uasset、目录注册及单元测试 |
| P5 | 角色工作室场景 | 以L_DBA_CharacterStudio现有无碰撞PreviewFloor为接触面，搭配前、中、近景分层；复用CharacterPreview流送生命周期；UI状态不直接持有Niagara | 真实.umap/脚本、选择/创建共享运行、退出清理 |
| P6 | 代码与蓝图对接 | 只在项目客户端既有模块增量增加环境预设启停、请求取消及参数驱动；制作可编辑蓝图/资源，不改动当前用户正在修改的Widget | 可复用接口与真实蓝图资源（若Monolith/Editor可用） |
| P7 | *全部资源和代码完成后*统一验证 | UE Editor/Client目标构建、资产回读/DataValidation、Automation、PIE/独立客户端、Cook/Stage与Dedicated Server剥离、Niagara性能；失败如实记录 | 带真实日志的验证报告 |
| P8 | 最后人工验收 | 在运行中的角色选择/创建界面观察四阶段：飘落、静置、贴地滑行、阵风扬起；检查挡脸、输入、画质和切换清理 | 人工验收清单、截图与未决问题 |

## 详细验收门槛
1. 贴图资源必须实际存在、透明像素有效、尺寸真实、可校验hash；不把样张、概念图或说明文档充当引擎贴图。
2. 材质/Definition/Niagara/蓝图/地图必须是真实由UE保存的二进制资产并成功重载；UE资产生成条件不足时标记“阻塞”，绝不伪造。
3. 一个客户端LocalPlayer在同一个角色工作室只拥有一组共享持续特效；选择/创建切换不重新创建；快速退出重进不复活旧请求。
4. 地面风吹首版仅依据预览关卡的固定平面位置/法线进行视觉接触判断；不修改PreviewFloor当前NoCollision策略、不启用Gameplay物理权威。
5. 复用GamePlatformVFXService的Play/Stop、已有世界缓存与性能门禁；Niagara实例和Definition租约按世界/播放器代次撤销。
6. 环境风只取视觉预设参数；服务器天气合同当前只提供WindIntensity，没有风向量，不发明新的服务器字段或逐粒子同步。
7. PC/移动GPU上预算需由真实采样决定，默认Ambient优先级；纯视觉资产不进专用服务器目标及最终Cook。
8. 所有英文标识均有中文注释；新增文件同一变更更新现行总体目录与受影响插件文档，避免重复文档真源。

## 风险和回退
- 当前工作区已有未提交改动：只新增明确的资源/代码路径并精准更新允许文件，禁止覆盖UI和未提交工作成果。
- 若Monolith未注册，不能修改任何UI Widget等界面视觉资产；VFX/材质/地图仅通过可验证的UE编辑器能力制作，否则只交付源纹理与可执行作者脚本并标记未完成。
- 源图可直接删除新增目录回退；代码增量以git diff逐文件审核；地图必须先保留目标原件并尽量按Actor Label幂等新增。
- 若旧场景已有资源冲突/引擎API差异，拒绝覆盖并输出实际异常；编译通过不等于场景效果已人工验收。

# BirdFlock（通用鸟群VFX）

版本：2026-10-11｜位置：GamePlatformVFX（平台层，唯一视觉特效插件）  
状态：真实GPU Niagara、三种WorldDefinition、统一场景蓝图与正式Presentation/Data链路已实装并通过独立运行验证。P6完整构建/烘焙的最终结果见 `Docs/Implementation/BirdFlockP4P7Execution_20261011.md`；人工视觉验收尚待用户确认，不得据自动化宣告生产就绪。

## 一、系统定位与可复用边界

本功能复用现有 `GamePlatformVFXClient（VFX客户端模块）`，不新建同领域插件、不在 MobaCommon（MOBA通用层）建立环境鸟群实现。平台公共源码不得了解任何具体项目的海鸥、燕子、雄鹰身份。项目实际物种资源归属 `DBAPresentationPack_Core（神兽联盟公共表现内容包）`。

正式场景使用 `BP_DBA_BirdFlockAmbient（统一鸟群场景蓝图）`，内部仅一个 `UDivineBeastsBirdFlockSceneComponent（项目场景配置组件）`。组件不逐帧运行、不渲染鸟模型，只向项目表现服务提交物种、数量和锚点，进入既有 `GamePlatformPresentation → GamePlatformData资源租约 → GamePlatformVFX → Niagara` 链路。一个粒子对应一只鸟，GPU模块计算局部飞行轨迹，Mesh Renderer批量渲染，统一母材质按顶点RGB显示羽色、按Alpha控制翼尖振翅。

旧 `AGamePlatformBirdFlockActor / UGamePlatformBirdFlockProfile（ISM原型）` 仅保留为历史客户端原型和回归夹具，**不用于正式场景蓝图**，不建立第二个资源加载器或特效提供者。平台模板只引用引擎中立模型；项目鸟类网格/材质/身份全部在第三层。自然环境鸟群不进入MobaCommon，不复制每鸟位置，不改变服务器碰撞、伤害或导航。

## 二、配置接口

在统一蓝图的 BirdFlock 场景组件详情中设置 `SpeciesId（鸟种标识）`：Seagull（海鸥）、Swallow（燕子）、Eagle（雄鹰）；设置 `BirdCount（数量）` 为0–512，0发送正式取消请求而非保留隐形模拟。`FlockId（场景鸟群身份）` 应在本地玩家作用域内唯一，空值使用所属Actor身份。同一FlockId更换类型/数量会先取消旧实例，再以新参数启动。

蓝图接口为 `ApplyBirdFlock（应用当前配置）`、`StopBirdFlock（停止本群）` 和 `GetBirdSpeciesOptions（查询鸟种选项）`。服务接口为 `StartAmbientBirdFlock（提交鸟群请求）`、`StopAmbientBirdFlock（停止指定群）`、`StopAllAmbientBirdFlocks（停止全部）`、`GetRequestedBirdCount（读取已受理请求数量）`。最后一个是请求数，不得用它冒充实际GPU粒子数。

正式WorldDefinition参数白名单最多7项：`User.BirdCount`整数0–512；`User.FlockRadius`半径200–10000cm；`User.FlockHeight`相对高度200–10000cm；`User.FlightSpeed`速度10–5000cm/s；`User.FlockSeed`整数0–1000000；`User.FlightMode`整数0盘旋/1拉长闭合掠过/2八字；`User.WindInfluence`扰动幅度0–2000cm。振翅频率和振幅由物种材质实例持有，不伪称已开放User.WingBeatHz。

初始物种默认数量为海鸥32、燕子72、雄鹰5。场景组件覆盖优先于物种默认；每个本地玩家最多16群，所有实例仍进入现有环境预算/距离裁剪。未知鸟种、非法数量及非有限位置明确拒绝，已运行的合法请求保持不变。GPU实测覆盖三物种各0/1/32/128/512，共15组；有预算裁剪时实际可见数可能低于请求数。

## 三、资源规范和目录

- `GamePlatformVFX/Content/Environment/BirdFlock/Materials/M_GP_BirdFlock_Master.uasset`：平台级唯一通用母材质；默认参数仅依赖Engine通用纹理，不允许硬依赖具体游戏项目。
- `DBAPresentationPack_Core/SourceArt/BirdFlock/DBA_BirdFlock_Models.blend`：Blender 5.2可编辑源，带3个UV低面数鸟类，顶点色RGB区分表面，Alpha为翼尖变形权重。
- `SourceArt/BirdFlock/Meshes/SM_DBA_Bird_{Seagull,Swallow,Eagle}.fbx`：原始米制3D网格。模型 X 为前向、Y 为翼向、Z 为上。
- `SourceArt/BirdFlock/Textures/T_DBA_Bird_*_Albedo_Source.png`：512²柔和低对比羽色原图，无刻意砂砾/颗粒纹。
- `Content/VFX/Environment/BirdFlock/Meshes/Flight/SM_DBA_Bird_*.uasset`：当前正式飞行网格，559顶点/1022三角面，修正共面反向翼面并保留源RGBA羽色/动画权重，FBX导入显式开启米到厘米转换。原始无后缀100倍大模型、旧`*_Cm`缺色模型和早期`*_Flight`微型导入测试资源均不是正式Niagara的引用目标，不可再次用于新场景。
- `SourceArt/BirdFlock/RefinedMeshes`：当前可重建飞行网格FBX、修正后的Blender源文件及SHA-256清单。
- `Content/VFX/Environment/BirdFlock/{Materials,Textures}`：三套实际材质实例和三套正式导入纹理。
- `Content/VFX/Environment/BirdFlock/Definitions/DA_DBA_VFX_BirdFlock_*`：三种真实`UGamePlatformVFXWorldDefinition`，统一Data逻辑ID归`presentation.dba.environment.birdflock.*@1`。
- `Content/VFX/Environment/BirdFlock/Blueprints/BP_DBA_BirdFlockAmbient`：真实蓝图、一个正式项目场景组件，可编辑鸟种/数量。
- `Content/VFX/Environment/BirdFlock/Niagara/NS_DBA_BirdFlock_*`：三种真实GPU粒子系统，均引用平台通用模块和环境EffectType，无逐鸟Actor。
- `Tools/Blender/BirdFlock` 与 `Tools/Unreal/VFX/BirdFlock`：源资产生成、正式UE导入/回读、鸟种预设、统一蓝图、隔离审核场景工具。
- `/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockReview`：已保存的直接Niagara视觉审核关卡。
- `/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockIntegrationReview`：已保存的正式表现链路审核关卡，三个统一蓝图实例通过真实LocalPlayer服务自动加载/播放。

## 四、资源生产操作

当前作者链路按以下顺序维护，不再运行旧ISM预设/地图作者脚本覆盖正式资源。

1. `RepairBirdFlockFlightMeshes.py（飞行网格修正）` 读取已有Blender源，删除反向重复面/退化面，保留UV/RGBA，输出独立RefinedMeshes。
2. `ImportBirdFlockFlightMeshes.py（正式飞行网格导入）` 使用UE内置FBX导入器、Replace源顶点色、ConvertSceneUnit厘米转换，保存Flight子目录真实StaticMesh。
3. `AuthorBirdFlockSpeciesMonolith.py --systems（项目Niagara变体作者）` 在已核实工程身份的Monolith中绑定三种新网格/材质、编译并保存；每种系统0错误/0警告。WorldDefinition首次创建使用`AuthorBirdFlockDefinitions.py`，已有定义不覆盖。
4. `AuthorBirdFlockSceneBlueprint.py（正式场景蓝图作者）` 创建并保存一个包含项目场景组件的蓝图。
5. `AuthorBirdFlockIntegrationReview.py（正式链路审核地图作者）` 在独立UE进程使用LevelEditor模板地图API创建新地图，不用普通AssetTools复制World。组件多字段必须原子设置，防止SCS重建后写入失效组件。
6. `ValidateBirdFlockP4Assets.py（真实资产门禁）` 在独立UE5.8进程回读平台引用、三种定义/系统/新网格、材质父类及蓝图实例，报告位于Saved/Validation/BirdFlockP4P7。

所有Python UE命令行的退出码0只说明进程结束；脚本异常有可能仍为0。必须同时检查 `LogPython: Error`、`LogEditorPythonExecuter: Error` 和输出的 `*_OK` 成功标记。

## 五、场景表现建议

- Village（新手村）湖面：海鸥低密度环绕和逆风滑翔。放置于水面上空，依照真实湖面高度设Actor锚点。
- 桃林古树：燕子群采用八字往复和短程掠过。若将来加入触发惊鸟，需从客户端表现事件驱动刷新预设，不让视觉鸟群碰撞影响游戏逻辑。
- OpenWorld（开放世界）高崖：雄鹰使用高海拔、大半径、低密度的滑翔轨迹。
- CharacterSelect/CharacterCreate（角色选择/创建场景）：远景少量鸟群，必须避让角色主体与按钮区域；不能让视觉表现控制权威流程。
- 一般不在 MainArena（主竞技场）默认开启环境鸟群，以免增加PvP画面噪声和性能负担。

## 六、性能与验收

正式实现使用GPU粒子、网格渲染、材质顶点振翅以及既有Niagara EffectType/平台环境预算。目标设备的绝对GPU/CPU帧耗仍需独占硬件条件下测量；本轮工作站同时进行编译与Cook，不能把它的帧率当成产品性能基线。

`RunBirdFlockGPUReview.py` 在独立真实图形编辑器输出五张1920×1080视口截图，并按三物种×五种数量读取实际Niagara GPU缓存；最终15/15匹配且位置有限。`RunBirdFlockIntegrationReview.py` 在真实PIE中验证蓝图自动启动、正式Data/Presentation请求链路、15组GPU数量、非法输入不污染、StopAll和世界清理。两类测试都不等于用户人工审美确认。

验证顺序：
- Client Win64 Development：检查UHT + C++ + 链接。
- Editor Win64 Development：已安全关闭本轮开始时无未保存资产的空闲编辑器并成功构建六个受影响模块；全Editor受NoEngineChanges保护拒绝改写引擎输出的记录单独保留，不以定向模块编译冒充全引擎重建。
- Server Win64 Development：检查没有带入ClientOnly鸟群代码或纯项目鸟类资源。
- 资产重载：独立进程验证平台四项资产、三物种定义/系统/网格/材质和真实蓝图父类/组件/选项，平台没有反向引用项目资源。
- 客户端自动化实测：最新Win64 Client执行`BirdFlock`过滤器，数量/散列、ISM原型回归和正式三物种目录映射共3项通过、0失败；报告`Saved/Validation/BirdFlockP4P7/Automation/index.json`。真实GPU数量由上述两套独立图形验证覆盖，不能只引用原型单测。
- 功能验证：蓝图切换鸟种、数量0/1/32/128/512、切换风/路径、离开世界后无残留。
- 人工视觉：审核Blender预览和UE真实关卡，确认羽色、层次、形变、翼尖频率、合理朝向、不穿地面/水面/建筑物、不会遮挡角色和UI；主观审美必须由人确认。

## 七、未完成与限制

- 当前三种鸟类为中远景低面数资源，不是近景影视级羽毛角色；本轮修正羽色丢失、反向重复翼面和单位错误，不宣称已达到最终3A美术验收。
- 轨迹是有界盘旋/拉长闭合掠过/八字，不是完整Boids邻居避让；安全飞行空间由场景配置负责，不自动躲避所有地形/建筑物。
- 村庄/开放世界及角色选择/创建的生命周期代码已接入；完整登录准入/跨服网络链路不属于本鸟群隔离测试的通过范围。
- P6构建/烘焙/长稳运行最终结论以续执行记录和真实日志为准。P7始终等待人工检查实际场景的密度、轮廓、遮挡、飞行高度与流畅度后签署，未由人确认前不标记全部完成。

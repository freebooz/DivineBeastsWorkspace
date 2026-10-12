# Surface Impact Marks（表面命中痕迹）使用与验收

## 所属范围
本能力属于 `GamePlatformSurfaceClient`（平台客户端通用层），独立于现有雪/苔藓/湿润 MPC 状态，不实现服务器伤害、碰撞几何破坏、真实 Chaos 断裂或永久存档。五类是 Bullet（弹孔）、Scorch（爆炸焦痕）、Slash（刀砍痕）、Fluid（液体/能量飞溅）、Crack（墙地裂痕）。第三层神兽联盟默认使用蓝色能量/墨迹风格，不承诺写实血液。

### 输入
- 命中事件应来自经过应用层判定的 `FHitResult`（命中事实）；需真实物理材质、命中法线、稳定的事件GUID和方向。
- `UGamePlatformSurfaceMarkBlueprintLibrary::SubmitSurfaceMarkFromHit`（蓝图提交命中）或 `SubmitSurfaceMark`（直接提交表现请求）。二者不改变任何权威 Gameplay 状态。
- `SurfaceType_Default` 提供类别内回退；配置中完全缺少某一类别时目录整体拒绝，避免错误材质悄然替代。
- `IGamePlatformSurfaceMarkService::AddSpawnedHandler` 在真实贴花组件生成后回调。现有 Niagara、SFX 可自行消费这一纯表现事件；Surface 不直接依赖 VFX 插件。

### 生命周期与性能
- `UGamePlatformSurfaceMarkWorldSubsystem` 每世界独立存在，ClientOnly；Dedicated Server 没有模块，也不能在世界内创建实例。
- `UGamePlatformSurfaceMarkCatalog` 及五类材质均通过现有 GamePlatformData 普通资源租约异步加载，无 `LoadSynchronous` 或自建 StreamableManager。
- `Pending` 仅代表排队，`Spawned` 才代表成功生成客户端 Decal。队列默认32，活跃上限默认128，重复命中GUID有界保留256；目录可以缩小预算，失效/旧代次不会重新安装。
- 到期时 Decal 按生命周期自动销毁，后续请求进行懒惰清理；不用每帧 Tick。相同世界远距/屏幕面积需与美术品质档位联合评估。
- 带命中组件的痕迹附着；移动组件销毁、世界切换与网络退出均不保留跨世界贴花。
- 需要真正裂开的墙/地，应由项目权威可破坏物体和 Chaos Geometry Collection 独立实现；贴花只表达视觉纹理。

## 真实资源
- 15张开发预览源图位于 `SourceArt/Marks/`，由 `Tools/Unreal/Surface/GenerateSurfaceMarksSourceArt.py` 可重复生成；`SurfaceMarkSourceManifest.json` 为 SHA256 单源证据。源图 1024×1024 Albedo RGBA / Normal / ORM 不是可运行 UE 资产。
- 真实资产由 `Tools/Unreal/Surface/AuthorSurfaceMarksUEAssets.py` 在 UE5.8 Editor 导入。母材质 `/GamePlatformSurface/Marks/Materials/M_GP_SurfaceMark_Master` 必须为 `Deferred Decal`，分别连通 BaseColor、Opacity、Normal、Roughness；五材质实例分别映射三张Texture2D。
- 目录 `/GamePlatformSurface/Marks/DA_GP_SurfaceMarkCatalog` 保存五类软材质引用和预算，客户端 Settings 默认软路径指向该标准身份。尚无真实UE目录或母材质时不会伪造加载通过。
- 当前生成纹理为可替换的风格化开发预览，正式摄影写实美术须独立签收。贴花无法改变碰撞和真实网格轮廓。

## 独立演示
- 脚本 `Tools/Unreal/Surface/AuthorSurfaceMarksDemoMap.py` 真实创建 `/GamePlatformSurface/Maps/L_GP_SurfaceMarks_Review`，五块引擎Cube测试板、地面、DefaultPawn、主光和 `AGamePlatformSurfaceMarkDemoActor`。
- Actor BeginPlay后延迟1秒对五块板真实射线检测；按相同生产表面服务提交，不绕过软资产租约。
- 在**唯一主工程**里直接打开该地图并使用Editor PIE或 `UnrealEditor.exe <uproject> /GamePlatformSurface/Maps/L_GP_SurfaceMarks_Review -game -DBAPluginDemo` 观察，不新建DevHost或第二项目。现有开发总览集成必须另核对，不修改其他会话正在维护的Demo脚本。
- 最终人工检查：五类颜色、透明边缘、法线、贴花方向、附着运动、预算淘汰、渐隐、移出/切图清理、蓝色液体替代风格、移动/高低画质、Server剔除。

## 三层接线
- 平台层 `GamePlatformSurface` 只管理普通命中痕迹；`GamePlatformVFX` 由外部订阅成功痕迹事件产生瞬时火花/烟尘/墨迹粒子。
- MOBA层消费已确认的命中事实，决定是否在真实世界静态表面留下痕迹；不把角色受击简单当成墙体碰撞，不得默认对所有角色命中都生成地表贴花。
- DivineBeasts（项目层）依据技能/命中材质映射 Kind/表面和风格；正式数值、生肖技能、镜头震动、音效、Chaos权限均保持原系统所有权。

## 证据边界
纯 Python 和 C++ 静态测试只能证明合同；完整 UHT/UBT 编译、自动化 Editor 运行、材质图编译、真实资产反序列化、演示截图、Client/Server Cook 是不同门禁。不得因文件存在将未运行项标记为已通过。

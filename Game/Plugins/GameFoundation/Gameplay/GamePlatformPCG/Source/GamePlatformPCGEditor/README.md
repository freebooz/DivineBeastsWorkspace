# PCG编辑器模块阶段断点

2026-09-21。按用户转入第九插件的指示提前收口；本目录是已写源码，不是UE验收通过。

## 当前实现与文件

- `GamePlatformPCGEditor.Build.cs`：与现有描述中的编辑器模块同名，声明PCG、runtime公开接口、UnrealEd、AssetRegistry和Projects依赖。
- `Private/GamePlatformPCGEditorModule.cpp`：默认模块注册，不自动创建世界或执行生成。
- `Private/Authoring/GamePlatformPCGEditorLibrary.h/.cpp`：反射工具入口；首次创建固定开发图和配置，已有任一目标则整批拒绝。不覆盖已有人工资产。部分保存失败保留现场并返回错误，不假装事务回滚。
- `Private/Authoring/PCGDevelopmentGraph.h/.cpp`：真实原生CreatePointsGrid→TransformPoints→DensityFilter→StaticMeshSpawner→output.Out；CPU、Weighted单网格、无属性旁路，descriptor变更和跨数据合并均关闭。
- `Private/Manifests/PCGSourceFingerprint.h/.cpp`：递归AssetRegistry包依赖与实际包/伴随文件字节、PCG及本插件源码/描述、Build.version与引擎版本参与摘要。脏包、未知依赖和缺源码拒绝；不是仅路径/Seed哈希，也不是密码学签名。
- `Private/Tests/PCGEditorTests.cpp`：来源摘要顺序/变化及两用途真实原生图形状的UE自动化测试源码，未执行。

## 实际能力边界

`CreateDevelopmentAssets`只在未来明确调用时创建以下四个包：

- `/Game/Development/Foundation/PCG/Graphs/PCG_Cosmetic`：装饰原生图。
- `/Game/Development/Foundation/PCG/Graphs/PCG_StaticCollision`：静态碰撞原生图。
- `/Game/Development/Foundation/PCG/Profiles/DA_PCG_Cosmetic`：编辑器静态装饰配置。
- `/Game/Development/Foundation/PCG/Profiles/DA_PCG_StaticCollision`：编辑器静态碰撞配置。

两个配置都为EditorGeneratedStatic，不是运行时装饰入口。网格使用引擎Cube；无地形、坡度、排除体积或任意外部输入支持。固定初始参数只作为开发夹具，区域身份仍需World正式接入。

`InspectProfileSource`只接受已加载且已保存的配置/图/网格，调用runtime公开图检查后返回来源记录。摘要范围为磁盘源包及已列明源码，不承诺覆盖虚拟化外部bulk存储、整个引擎补丁二进制、Cook产物或运行环境配置；不得直接批准生产内容。工具需要独占创作进程，不能与其他写包过程并发。

## 明确未交付与验证

没有运行任何资产创建入口，没有新增`.uasset`或`.umap`。没有PCG执行、地图生成/保存、ClearPCGLink转换、精确静态输出清理、独立重开、碰撞探针、Cook或Stage。原先准备的Operation/Poll/Cancel/Verify/Cleanup声明已撤回，不留下未定义方法或固定成功实现。没有资产Python脚本。

UHT、C++编译/链接、UE自动化均未执行。既有工程描述扫描阻塞历史不是本模块运行证据，也不能由静态核对宣布已解除。只进行了源文件/声明与引擎公开API核对。运行生成继续由runtime负责人维护，本模块未修改其代码、profile、inspection或manifest。

续作顺序：先解除经授权的工程前置并构建Editor；运行上述两项自动化；再增加显式原生异步生成操作及持有期；复用InspectOwnedOutput的pointcount==ISM检查；仅成功后保存/归属转移；实现精确清理、跨进程重开与碰撞核验。任何清理必须先取消并证明可访问/排空，不能复制引擎私有实现或越过Data租约所有权。

总体目录规划/根文档按本次写入边界未改，需由整合负责人登记这些文件。本目录未获人工审查。

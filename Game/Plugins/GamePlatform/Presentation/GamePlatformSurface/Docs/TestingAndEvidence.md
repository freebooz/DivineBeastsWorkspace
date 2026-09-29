# GamePlatformSurface 测试与证据

## 自动化源码

- `GamePlatform.Surface.State.ClampAndCompare`：有限值、范围裁剪、同值比较和绑定缺失语义。
- `GamePlatform.Surface.Editor.MPCContract`：用 Transient（瞬态）MPC验证八个稳定标量参数及缺项失败路径，不写工程资产。

## 结构基线

新增后现行基线为：GamePlatform平台层39个；加 MobaCommon 中保持 GamePlatform 稳定身份的 `GamePlatformArena`后，GamePlatform稳定身份40个；独立 `MobaPresentation` 1个；DivineBeasts代码插件5个；代码／机制合计46个，真实内容插件N另计。

2026-09-27历史记录也出现过“46／40”，但当时成员包含后来退休的 `GamePlatformOpenWorld`，不包含本插件；历史通过结果不能冒充当前 Surface 验证。

## 推荐验证顺序

1. DesignBaselineAudit 行为测试。
2. 正式工作区 `ValidateDesignBaseline.ps1`。
3. UE5.8 Editor Target 编译 Client／Editor模块。
4. 运行 `GamePlatform.Surface.*` Automation。
5. 执行 `GamePlatformSurfaceCoreAssets`创建标准MPC，再用 `-ValidateOnly`回读。
6. Material Editor真实制作母材质／材质函数后执行Shader编译与人工Review。
7. Client Cook／Stage验证资产存在；Server Cook／Stage验证纯表现资产被剥离。

编译不能证明材质图正确；MPC创建不能证明雪／苔藓／湿润／积水效果已经制作；静态Target声明不能证明Server产物已剥离；这些证据必须分开记录。

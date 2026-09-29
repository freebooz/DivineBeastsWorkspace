# GamePlatformSurface 架构说明

## 定位

`GamePlatformSurface（游戏平台通用环境表面材质插件）`属于 GamePlatform 第一层的 Presentation（表现）分类，解决跨项目稳定问题：统一环境表面状态、用每世界 MPC 高效广播全局材质参数、固化通用材质资产合同，并由 Editor 工具真实生成／验证引擎资产。

## 模块边界

```text
GamePlatformSurface
├── GamePlatformSurfaceClient      # ClientOnly：状态、服务、MPC桥接、蓝图入口
└── GamePlatformSurfaceEditor      # Editor：真实资产生成、校验、自动化测试
```

没有共享服务器职责，因此不创建 Runtime／Server 空模块。Client 模块仅依赖 UE Core／CoreUObject／Engine／DeveloperSettings，不依赖 PCG、VFX、World、MobaCommon 或 DivineBeasts，保证其他游戏可单独复用。

## 数据流

```text
天气／世界事件／项目表现事实
        ↓
FGamePlatformSurfaceEnvironmentState
        ↓
IGamePlatformSurfaceService
        ↓
UGamePlatformSurfaceWorldSubsystem
        ↓
UMaterialParameterCollectionInstance
        ↓
平台母材质／项目材质实例
```

状态变化才更新 MPC，不轮询。PIE 多世界各自维护独立实例；世界销毁时清理委托与绑定。

## 权威边界

Surface 只表达视觉：湿润不能决定滑倒或移动速度；积雪不能决定服务器道路阻塞；温度不能替代服务器环境规则；积水视觉不能替代真实 Water／Physics（水体／物理）。需要影响玩法的环境事实继续由服务器规则、World Definition、GAS、导航等权威系统拥有。

## 相邻插件

- `GamePlatformWorld`：世界身份、区域、流送、就绪，不拥有表面 Shader（着色器）算法。
- `GamePlatformPCG`：生成协议、模板和图，可选择材质，但不实现雪／湿润算法。
- `GamePlatformVFX`：Niagara 动态表现，不持有长期地表材质状态。
- `DBAWorlds`：神兽联盟世界定义和组合，不把项目字段写回平台。
- `DBAWorldPack_*`：拥有项目纹理、材质实例、世界场景和专属材质资产。

## 材质层建议顺序

`Base Surface（基础表面） → Macro Variation（宏观变化） → Moss（苔藓） → Snow（积雪） → Wetness（湿润） → Puddle（积水） → Final Material Attributes（最终材质属性）`。

各层至少同时考虑 Base Color（基础颜色）、Normal（法线）和 Roughness（粗糙度），不能只染色冒充真实覆盖层。

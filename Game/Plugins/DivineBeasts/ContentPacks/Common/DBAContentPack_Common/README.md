# DBAContentPack_Common（项目公共角色内容包）

## 职责与归属

第三层DivineBeasts纯内容包，稳定挂载点为 `/DBAContentPack_Common/`。持有项目共享的Manny/Quinn开发角色网格、骨架、物理资产、材质纹理与预览/世界移动动画。无C++模块，不承担平台机制或权威技能规则。

## 已有内容与入口

`Content/Mannequins/DBA/` 包含项目使用的网格、ABP_DBA_PreviewIdle、ABP_DBA_WorldLocomotion、BS_DBA_WorldLocomotion及实际移动序列；`Content/Mannequins/Standard/` 与 `UE5/` 保留既有依赖。
本轮文件系统观察为49份.uasset；数量只表明库存，不证明每份资源当前已加载、Cook或视觉通过。具体运行证据见项目 `Docs/Implementation/AnimationHUDDevelopmentValidation.md` 与 `Docs/Architecture/新手村玩家可见性与移动动画修复记录.md`。

## 加载、服务器与变更边界

角色外观由项目表现适配读取，运行加载/租约归既有GamePlatformData。当前包内容服务客户端表现；服务器权威位移和技能定义归已有代码/定义契约，不能因包名Common推断必须整包加载。实际服务器剔除以统一规则和容器证据为准；今后若新增服务器必需碰撞/动画，必须先调整精确保留策略与回归。
不要把开发人体模型称作正式生肖美术；不批量改名/迁移稳定骨架、动画或资源引用。真实.uasset只能通过UE工具创建和保存。

## 验收入口

`Tests/Assets/ValidateWorldLocomotionAssets.py`（真实引擎资产与姿势），`Tests/Architecture/ValidateDesignBaseline.ps1`（内容登记），以及 `Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json` 的内容交付/界面场景。源码、库存、UE加载、Cook与人工画面须分别留证。

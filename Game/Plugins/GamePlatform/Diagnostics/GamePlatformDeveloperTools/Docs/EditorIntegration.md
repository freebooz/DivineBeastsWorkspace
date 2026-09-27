# EditorIntegration（编辑器集成）

模块启动时仅在非 Commandlet 环境注册 Tools > Game Platform Validation（游戏平台验证）菜单。

菜单规划：Validate Selected、Validate Folder、Validate Project、Validate Architecture、Audit Server Assets、Audit Client Assets、Run Review Cases、Performance Tests、Open Latest Report。

资产右键 Validate Assets 继续复用 UE Data Validation 原生菜单，不重复实现同职责 Content Browser 菜单。
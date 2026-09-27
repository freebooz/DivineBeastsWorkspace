# GamePlatformCharacter（游戏平台角色插件）

跨游戏角色Definition、基础移动/出生包络、角色初始化契约、角色创建Provider接口以及统一Definition异步加载。Server-safe Definition不硬引用Mesh/VFX/SFX/UI/具体Ability类。

本轮补充RequiredDefinitions空值、自引用、重复依赖校验与自动化测试。真实Spawn/Possess/复制仍由上层流程组合验证。

# P0插件公开接口候选审查

> 从真实Public头文件扫描 UFUNCTION（蓝图可见反射函数）标记，属于开发计划和进一步接口核验输入；**不等于实际完整功能或UI可交互演示已经实现**。

扫描含公开头文件 508 个，UFUNCTION声明候选 592 项。

| 插件 | Public头文件数 | UFUNCTION候选 | 状态 |
| --- | ---: | ---: | --- |
| DBAContentPack_Common | 0 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| DBAFrontEndPack | 0 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| DBAPresentationPack_Core | 0 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| DBAUIPack_Core | 0 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| DBAWorldPack_Village | 0 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| DBAArena | 14 | 12 | 待按真实行为拆分FeatureId并运行验证 |
| DBAClient | 87 | 217 | 待按真实行为拆分FeatureId并运行验证 |
| DBAGameplay | 19 | 20 | 待按真实行为拆分FeatureId并运行验证 |
| DBAServer | 2 | 2 | 待按真实行为拆分FeatureId并运行验证 |
| DBAWorlds | 4 | 2 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformApplicationFlow | 6 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformInput | 5 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformLoading | 3 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformLocalization | 0 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformSave | 3 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformSettings | 10 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformDebug | 3 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformDeveloperTools | 10 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformTelemetry | 11 | 1 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformCore | 6 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformData | 9 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformLobby | 0 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformVillage | 0 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformAI | 12 | 13 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformAbilitySystem | 12 | 2 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformAnimation | 3 | 6 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformCharacter | 6 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformCombat | 18 | 18 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformGameplay | 22 | 11 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformQuest | 9 | 4 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformOnline | 8 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformServer | 2 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformSession | 2 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformCommerceUI | 6 | 8 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformEntitlement | 7 | 6 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformEquipment | 8 | 5 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformInventory | 4 | 5 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformLiveOps | 5 | 10 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformProgression | 7 | 5 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformCamera | 1 | 3 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformPresentation | 6 | 7 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformSFX | 3 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformSurface | 6 | 3 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformUI | 58 | 154 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformVFX | 28 | 6 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformInteraction | 13 | 27 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformNavigation | 9 | 6 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformPCG | 20 | 9 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformWeather | 6 | 12 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformWorld | 8 | 0 | 待按真实行为拆分FeatureId并运行验证 |
| GamePlatformArena | 19 | 17 | 待按真实行为拆分FeatureId并运行验证 |
| MobaPresentation | 8 | 1 | 待按真实行为拆分FeatureId并运行验证 |

## 必需人工补证

1. UFUNCTION是可被蓝图调用的声明，不保证实现功能完整、已经启用或能独立运行；许多纯C++服务也不会标UFUNCTION。
2. 每个视觉/交互功能需要映射真实C++/蓝图实现、输入参数、资源、功能预设、失败/重置和演示场景；可复用同一地图，不可只展示函数名。
3. 现有开发地图需通过UE资产注册表、实际PIE和人工截图/性能复核；只有通过后从候选转为合格演示。
4. 服务型插件可用诊断台而非三维关卡；ServerOnly（仅专用服务器）能力需真实服务器证据。

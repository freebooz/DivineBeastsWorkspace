# AssetManagerAndLeases（资产管理与租约）

本轮没有真实Project/ContentPack表现资产，因此DivineBeastsPresentation没有新增GamePlatformData硬依赖，也没有创建DivineBeastsAssetManager。

当前实现的所有Registration（注册）和Logical Preload（逻辑预加载）都有FGuid所有权句柄，并在Pack停用、World切换、Character/Account切换时撤销。

未来真实Provider需要加载PrimaryAssetId/SoftRef/Bundle时，应复用GamePlatformData + UAssetManager，并把实际Lease（资产租约）留给VFX/SFX/UI/Animation/Camera Provider；项目Presentation只协调逻辑需求。

真实资产Lease释放测试当前未执行。

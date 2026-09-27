# TestingAndEvidence（测试与证据）

C++ Automation源码覆盖Handle、Catalog、Resolver、参数Schema、10种Behavior、Composite和预算等基础能力。新增测试验证World句柄身份、未知/越界参数拒绝、同级Catalog歧义拒绝和全部Behavior Definition类型。

PowerShell生产门禁包括 TestGamePlatformVFX、TestVFXCatalog、TestVFXWorldLifecycle、TestVFXScalability、TestVFXCook 和 VerifyGamePlatformVFX。静态可验证项允许通过；Multi-PIE、ClientTravel、Review Map、真实Cook和性能在没有环境/工件时必须保持未执行。

本轮最终静态证据：TestGamePlatformVFX（平台VFX生产门禁）77/77通过；TestVFXCatalog（目录/解析器门禁）19/19通过；TestVFXWorldLifecycle（世界生命周期门禁）15/15通过；TestVFXScalability（伸缩门禁）9/9通过；VerifyGamePlatformVFX（综合验证）55项无失败；三层插件架构与DeveloperTools门禁均通过；附件指定46份专题文档全部存在。最终综合RunId为 `f404874d220c4d2a92c64821e66f6837`。

最低48项自动化按证据分层：模块/依赖/审计/Provider/World隔离/Preload Lease/Scalability静态边界可标“通过”；需要UE Automation、Cook、Review Map、Multi-PIE、Travel或性能实测的项目继续保持“未执行”。

“源码存在”“测试源码存在”或“ClientOnly声明存在”均不能替代UE5.8编译、Automation和Cook证据。
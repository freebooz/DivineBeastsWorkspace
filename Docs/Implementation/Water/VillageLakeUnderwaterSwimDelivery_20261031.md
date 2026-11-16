# SSLM 新手村湖泊、游泳潜水与水下表现交付

用户已批准设计；本轮范围为游泳、自由潜水和水下视觉，不包含氧气、溺水伤害。工作沿用正式工程及当前分支，保留既有地图、AI、植被和界面修改；没有提交、推送或清理历史资料。

## 实现

- 新手村继续使用真实 `/Script/Water.WaterBodyLake`，保留原湖体GUID、24控制点、湖岸、河流和两个出生点。18,000点真实地形复核最大差0cm；禁雕刻设置保留。
- `Village.Lake` 绑定从同一湖岸烘焙的CPU PhysicsVolume，深度2500cm覆盖真实湖底。凹岸之外、高地、水面以上及体积底部之外保持干态；孤立、未注册、重复ID被拒绝者不能授予游泳。高优先级禁泳Region继续服从唯一WaterWorldSubsystem规则。
- 角色复用原生CharacterMovement预测、复制和服务器运动：水中W/S沿视线，A/D横移，Space上浮、LeftCtrl下潜；岸上原有运动保留。速度初值280cm/s、浮力1.1，最终平衡待人工。
- 水下后处理保留引擎水线、吸收与方向散射，并在同一原生Water后处理图追加受湖岸、深度、接收面及距离约束的动画焦散。Low关闭新增焦散；Medium/High/Epic强度为0.35/0.7/1。中高档只改变强度，不声称降低采样成本或GPU达标。
- 当前12份玩家Profile共用Kachujin骨架、网格和ABP。新增四份真实泳潜序列及6采样2D BlendSpace，现有动画图按只读`bIsSwimming`切换，并保留跳跃、DefaultSlot施法及死亡覆盖。
- 既有 `/Game/Development/Water/L_GP_WaterReview` 对应Water插件测试页，增加数据型审核Pawn/GameMode，直接复用主线角色；页面的人工步骤说明泳潜按键。没有创建第二套测试中心或运动器。

## 当前证据

证据根：`Saved/Validation/VillageWaterSystems20261031`。

| 阶段 | 结果与边界 |
| --- | --- |
| 原生创建/保存/冷读 | Village及WaterReview精确区域、体积、水下材质通过；动画四序列、6采样BlendSpace、ABP UpToDate。 |
| 原生回归 | Water两项、动画两项、泳潜输入一项通过。最新Water用例有2项隔离World销毁无上下文警告，输入用例有既有GAS扫描警告；无测试错误。 |
| 作者/过滤回归 | 材质脏包保护2/2、服务器表现过滤34/34通过。 |
| 完整目标构建 | Editor `c044dafb-490a-4d63-bda4-eaded00c2d7e`；Client+Server `b2d9fef8-4f7f-467b-98b5-d65c4d788127`通过。固定UE5.8 Win64/Development参数，未Clean/Rebuild。 |
| 实际本地运动 | `RuntimeProbe-b256ea37/RuntimeSwimming.json`五项通过：真实Chaos入水、下潜、上浮、离水清理、动画一致；不强设Swimming。直接提交运动输入的显式本地PIE夹具不代替真实键盘焦点、联机或弱网回归。 |
| 原生画面 | `VillageLake.Top.png`、`VillageUnderwater.Final.png`、`VillageSwimming.Final.mp4`。视频由真实原生帧按3.2fps合成，不能作为帧率或性能测量。 |
| 不限帧 | 原生回读`t.MaxFPS=0`、`r.VSync=0`。未保存玩家偏好；PIE不作设备性能结论。 |
| UI颜色 | 本轮真实D3D12 `UIColorRHI.json`通过，字体修改后的五种尺寸截图/输入指纹可追溯。 |

## 一次独立审查与修复

fresh `water_final_review`发现3项Important，均补失败回归后修复：重复ID被拒Actor的体积借同名资格；普通PhysicsVolume重叠歧义；材质作者重跑覆盖脏包。`ReviewRegressionResults.json`与材质RED/GREEN日志保留原始失败及通过结果。按执行技能只进行一次整体审查，没有追加复审。

目录职责登记已同步；没有遗留未处理Minor。审查未代判氧气/溺水、其他骨架、速度/浮力平衡、泳姿审美和新增审核Pawn的人工结果；这些边界保留。

## 实施裁定及代价

1. 从Engine公开UBrushBuilder填写原生Model/Polys，由UActorFactory烘焙BSP/BodySetup，规避UEditorBrushBuilder跨DLL未导出虚函数。仍依赖锁定引擎作者接口，升级时须回归。
2. UE5.8材质三个参数Setter写入后固定返回false，因此以参数存在、真实回读、保存、Shader和冷读判断；不修改引擎。升级引擎后须重核行为。
3. 精确区域采用APhysicsVolume薄适配，调用唯一水域索引并检查注册Actor指针；未开始Play的单元World只验证资格/原生通知，完整Broadphase由真实PIE证明。单元测试本身不证明网络。
4. 作者对未知PhysicsVolume采用AABB保守拒绝；可能需要人工消除实际不相交但包围盒交叠的冲突，不自动改动既有体积。
5. 审核图采用数据型Pawn/GameMode以支持实际泳潜，主线执行逻辑唯一；只在开发地图生效，不提供正式身份或准入豁免。

## 待验收

Cook/Stage、实际容器、运行服务器版本与准入证据在下方最终发布记录中更新。四档GPU/透明Overdraw测量、真实双端泳潜/弱网修正、人工视觉和键鼠签收不能由源码、截图、启动或PIE测试代填；`HumanAccepted=false`。

既有蕨类PCG放置器未注册WorldDirector的地图验证错误保留为原有问题；本轮没有修改它，也未宣称整张地图DataValidation全部通过。

## 最终发布与运行记录

- Server Cook/Stage：`345bc0e8-1feb-456b-9d86-bc9ac5f71724`通过；三份实际容器全检通过。实际专服地图保留Region、精确PhysicsVolume和Village.Lake；视觉WaterBodyLake、水下材质及遮罩名未进入该地图。`WaterContainers.json`进一步核对客户端9项水下/泳姿/ABP唯一导出，服务器相应纯表现导出为0。
- Client Cook/Stage：`b96661d9-e54f-4b04-a8f6-20b3f134e552`通过。首次0510f9cc原生Cook及入包成功，但既有同次Niagara编译证据门禁失败；按原入口`ForceNiagaraCompileOnLoad`重跑后63项依赖及全部15系统同次编译通过，没有修改或绕过门禁。
- PluginDemo Client的11张实际编目地图Cook/Stage：`96a2cb69-f245-48b2-a066-5e131357263c`通过，包含更新的Water审核图。源码独立页仍以当前作者Manifest及Catalog指纹为准；不把本Cook当逐插件人工通过。
- 最终独立冷读PID7844：审核Pawn的Pitch=0/Yaw=-90、主线ABP、6采样BlendSpace、GameMode、原生水域、水下父材质和线性遮罩均通过，脏包0；Catalog指纹`587e4980e4708cb74ead56ec219448c9b42a88a5d289f8c7604bb5b6553f3ac7`与作者源相同。
- 发布兼容性核验绑定501份共享源码、360份权威资产、144份视觉资产以及真实Stage中的可执行/DLL字节；战斗版本SHA256=`e3bf19c2454837a00e310bfb5fc4387c9dbf5f6cccd2a463e2e929272c8755e9`。
- 已启动同一Server Stage的Village PID38960/UDP17782与MainArena PID23188/UDP17881，进程路径、启动身份、映像SHA及引擎就绪/端口归属通过。两个Client PID25100/10696来自本轮Client Stage，均停留手动登录页，未自动登录；实际日志回读t.MaxFPS=0、r.VSync=0。
- `RuntimeCompatibility.json`、`RuntimePlan.json`仍标记真实准入未验证及人工未签收。没有把登录页、进程就绪或无复制错误启动日志冒充实际Village/MainArena准入；双端泳潜/弱网、四档GPU及人工视觉仍待验证。

最终页面接线：PID7028使用正常DBAGameInstance和显式插件演示模式，通过同一Catalog.OpenScenario(GamePlatformWater.visual.Manual)完成Hub到WaterReview的真实导航；目标页已打开，28秒后仍为目标地图，自动生成BP_DBA_WaterReviewCharacter，MoveIgnored=false。`WaterPageNavigation.json`记录实际世界/Pawn/控制器；这比脚本手动切图多验证了真实页面导航，仍不代替鼠标键盘与人工签收。当前编辑器保留该PIE场景供人工观察。

演示包实际IoStore追加检查：`PluginDemoContainers.json`核对11张地图及审核Pawn/GameMode、水下实例/遮罩、最新Catalog，共16项唯一有效导出，全部通过。

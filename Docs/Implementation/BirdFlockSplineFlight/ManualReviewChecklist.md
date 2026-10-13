# 《SSLM》Spline鸟群 BF-P8 人工视觉与交互验收单

> 范围：UE5.8 正式 GamePlatformPresentation → GamePlatformData → GamePlatformVFX Niagara 鸟群。
> 状态：**待审核**。本单不能因代码存在、资产保存或旧版本截图自动标记通过。
> 设备、构建、地图、版本、审核人等由实际人工验收填写；保留失败证据。

## 一、审核环境信息

- 当前 Git 提交：
- Unreal Engine 版本：
- 测试构建目标：Editor / Client / Server：
- 运行模式：PIE（编辑器内运行）/ 独立客户端：
- 测试设备 GPU / VRAM / CPU：
- 分辨率、画质与帧率限制：
- 审核日期、审核人：
- 记录目录：Saved/Validation/BirdFlockSplineFlight/（由实际RunId区分）

## 二、真实资源与身份检查

- [ ] 原海鸥、燕子、雄鹰三种正式 Niagara 系统、Definition及Flight模型全部可UE重载，源RGBA颜色未退化。
- [ ] 原七个User参数及Orbit=0、Traverse=1、FigureEight=2全部不变。
- [ ] 新Spline三个模式3/4/5明确使用相同正式Presentation/Data/VFX链路，不恢复旧ISM Actor。
- [ ] GPU Niagara Spline DI 确实被正确绑定到当前World的SplineComponent；无DI不能把八字飞行当作样条通过。
- [ ] 新增4个User参数存在、类型正确、真实Niagara模块输入绑定可读；Schema超限明确拒绝。
- [ ] 原Emitter State Self/Once/固定生命周期稳定，没有重复Burst造成持续增长。
- [ ] 仅通过UE/Monolith产生的新.uasset/.umap被独立回读；脚本成功码不替代反射类型/资产树检查。

## 三、视频核心群飞复现

- [ ] 单程Spline从给定生成边界出发、群体完整沿指定三维路径飞离并停止/回收。
- [ ] 循环Spline终点回到起点，无瞬间大跳、速度奇点或方向翻转。
- [ ] 往返Spline在终点平滑反向，鸟身朝向真实速度，不倒飞。
- [ ] 群体中心使用共享路径进度；不同个体仅有局部扰动，不会整个群散布在全条路径。
- [ ] 同一群速度/转弯/随机相位有微差，但形状可辨，鸟群自然不机械。
- [ ] 改变Spline控制点与安全高度后重新提交，已关闭的世界不保留旧Spline引用。
- [ ] 断开、删除或世界分区卸载Spline时明确停止或回退，不出现NaN、无限飞行或旧世界残留。

## 四、飞鸟形体与动画

- [ ] 三种鸟X轴朝前、尺寸符合厘米单位，侧面/底部视角轮廓正确；网格没有背面重叠与Z fighting（深度闪烁）。
- [ ] 海鸥/燕子/雄鹰颜色及轮廓能区分，正式Flight网格羽色RGB和翼尖权重A没有丢失。
- [ ] 每只鸟扇翅相位不同，翅根符合机械结构，不是鸟身整体上下震荡。
- [ ] 拍翼→滑翔→拍翼变化自然，飞行状态和转弯身体倾斜合理。
- [ ] VAT（顶点动画纹理）如启用，原始骨骼资产、顶点排序/UV、纹理精度和材质采样均经过真实UE检验；未启用则明确记录WPO回退。
- [ ] 多LOD距离切换没有突兀跳帧、透明排序错误、色差或错误Mesh/Shader。

## 五、分批生成、Boids及生命周期

- [ ] 单群数量0、1、32、128、512均正确，随机种子固定时可复现。
- [ ] 最多16个环境群组，每群不超过512，越界请求失败并保留原有合法群。
- [ ] 连续多轮生成/结束/循环/停止，实际GPU粒子数量稳定，不随轮次倍增。
- [ ] 群体分离/对齐/聚合使用有限邻域或网格，不得CPU逐鸟全对比O(N²)。
- [ ] 当GPU邻域数据不可用时强制安全降级且UI标示，不能称作已实现完整Boids。
- [ ] World退出、切图、同ID不同Owner、异步旧回调、取消/重入，全部回收自有实例与租约。
- [ ] 10分钟以上稳定性实测，最后ActiveNiagara/Particle/Lease均回归0或合理基线。

## 六、世界生态与演示中心

- [ ] 鸟群演示中心能从插件总览直达，无登录、无后端认证。
- [ ] 面板切鸟种、数量、路径、群聚权重、滑翔、批次启动停止/复位能够真实调用正式组件。
- [ ] RequestedCount（请求数量）、ActiveNiagara（系统数量）、GpuLiveParticleCount（GPU实测）分别展示；无法读回的值明确“待测”。
- [ ] BirdSplineReview真实地图鸟类、控制点、镜头及背景可看；用Monolith制作并编译/保存UI而非静态占位图。
- [ ] Village湖面/桃林客户端表现不穿水面/建筑、不遮挡角色/HUD/按钮。
- [ ] OpenWorld高崖仅在实际地图和真实锚点审核存在时放行；没有正式地图不得宣称完成。
- [ ] MainArena默认关闭背景装饰鸟群，竞技可读性优先。

## 七、性能和发行审计

- [ ] 实测CPU/GameThread/GPU Niagara毫秒、内存/显存、系统数量、GPU粒子数量、Shader采样。
- [ ] 目标机画质档位/距离剪裁真正降低负载；预算按项目实测标准，不虚构帧耗阈值。
- [ ] Client Cook/Stage容器存在所需正式Niagara、Flight网格、材质、Definition及系统变体。
- [ ] Dedicated Server Cook/Stage容器纯视觉BirdFlock、审核地图、源美术/新VAT纹理为0。
- [ ] 源Blender/FBX/历史ISM原型未被错误带入正式发行包。
- [ ] UE Editor/Client/Server编译与Automation实跑使用当前同一HEAD/构建身份，不用历史报告冒充新回归。

## 八、人工结论（严禁自动填为通过）

- 审核人：
- 日期：
- 审核范围与RunId：
- 结论：待确认（通过 / 需要修改 / 未通过）
- 对应真实截图/视频及GPU读回报告：
- 发现缺陷与修复任务：
- 遗留功能及阻塞：

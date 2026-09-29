# GamePlatformSurface 人工审核清单

## 架构

- ClientOnly／Editor双模块，无空Runtime或Server模块。
- 平台源码不出现DivineBeasts、生肖、MOBA身份。
- Dedicated Server不依赖Surface。
- 无Tick；世界退出清理委托和MPC绑定。
- 非有限输入拒绝，MPC缺失／参数缺项明确返回失败状态。
- Surface不决定碰撞、导航、移动、战斗或天气权威。

## 资产

- `MPC_GP_SurfaceGlobal`为UE真实资产，不是文本占位。
- 八个参数名、类型、默认值符合合同。
- 母材质和材质函数由 Material Editor真实创建、编译、保存。
- 项目纹理和 `MI_DBA_*`不进入平台插件。

## 视觉

- Snow：水平面覆盖明显、垂直面可控、边缘有噪声、高度规则可调。
- Moss：不是整片纯绿，宏观／微观变化清楚。
- Wetness：同时影响颜色、粗糙度和必要微法线。
- Puddle：优先水平／低洼面，不在墙面无条件形成镜面水层。

## 性能与交付

- 使用 Material Stats／ProfileGPU记录真实数据。
- Lite材质确实裁剪功能分支。
- Client Cook包含实际使用资产。
- Server Stage不含纯Surface材质资源或客户端模块。
- 关闭插件后，无关World、PCG、VFX和服务器玩法仍可运行。

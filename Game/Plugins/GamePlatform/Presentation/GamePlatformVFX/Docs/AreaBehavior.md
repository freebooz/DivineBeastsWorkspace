# AreaBehavior（区域行为）

Area用于范围提示、地面预警和持续区域视觉。范围尺寸来自权威/预测玩法事实，经Parameter Schema进入Niagara。

Ground align属于表现；Area绝不通过Overlap/Trace自行决定Damage。性能降级可以降低视觉成本，但不能改变真实范围与持续时序。
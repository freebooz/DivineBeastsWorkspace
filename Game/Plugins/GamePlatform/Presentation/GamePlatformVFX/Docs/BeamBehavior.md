# BeamBehavior（光束行为）

Beam端点由Definition Schema声明，LWC端点使用Niagara Position接口。端点更新来自事件或受控频率，不通过反射逐帧查参数。

Beam只表现持续连接/锁定；Damage和目标合法性属于Gameplay。大坐标Beam应声明LWC要求并由Editor Validator检查Niagara System。
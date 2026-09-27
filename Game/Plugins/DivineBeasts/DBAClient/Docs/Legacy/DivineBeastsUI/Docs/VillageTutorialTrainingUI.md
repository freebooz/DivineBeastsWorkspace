# VillageTutorialTrainingUI（新手村教学训练界面）

正式Village Experience：

- Experience.Village.Main
- Experience.Village.Tutorial
- Experience.Village.Training

Tutorial Guidance（教学引导）应展示来自权威Quest/Onboarding投影的Stage/Objectives；当前项目尚没有TutorialStage UI-safe投影，因此页面清单存在，但实际目标数据接线未执行。

Training Controls（训练控制）提供Command接口，但当前没有正式Training Reset业务Owner端口。组合根明确返回 Village.TrainingResetUnavailable，不会在客户端SetHealth、清Cooldown或GiveAbility。

Tutorial Complete（教学完成）必须等待服务器/PlayerData长期状态返回；UI不能本地SetTutorialComplete。

Village Main HUD复用World HUD基础投影。真实Village地图与UI资产当前仍为0。

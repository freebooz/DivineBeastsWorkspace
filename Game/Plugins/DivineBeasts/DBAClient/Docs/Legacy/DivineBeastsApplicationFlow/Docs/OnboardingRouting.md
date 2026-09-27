# OnboardingRouting（新手路由）

OnboardingState（新手状态）由 PlayerData 长期权威保存：New、TutorialRequired、TutorialInProgress、OnboardingComplete。

未完成引导的角色只允许请求 Village（新手村）体验；当前默认路由为 Experience.Village.Tutorial，可由后端/项目规则继续细分 Main/Training。完成引导后默认请求 Experience.OpenWorld.Hub，并允许 OpenWorld.Main。

客户端不写 OnboardingComplete。

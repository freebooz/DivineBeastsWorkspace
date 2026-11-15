package gameservercontrol

import "divinebeasts/backend/internal/modules/gameserver"

// LocalValidationInstances（本地验证实例快照）返回控制面当前观察到的测试角色及其启动代次。
func (s *Service) LocalValidationInstances() []gameserver.Instance {
	return s.registry.ValidationInstances(s.now().UTC())
}

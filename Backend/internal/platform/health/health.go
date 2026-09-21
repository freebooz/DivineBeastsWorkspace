// Package health（健康状态包）提供服务Readiness（就绪）状态聚合能力。
package health

import "sync"

// State（健康状态）保存数据库、缓存、消息总线等依赖项的检查结果。
type State struct {
	mu     sync.RWMutex
	checks map[string]bool
}

// NewState（创建健康状态）创建一个空健康状态。
func NewState() *State {
	return &State{checks: make(map[string]bool)}
}

// Set（设置检查结果）更新指定依赖项的健康状态。
func (s *State) Set(name string, ok bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.checks[name] = ok
}

// Ready（是否就绪）只有全部已注册检查项都成功时才返回true。
// 空检查集视为就绪，便于无外部依赖的开发环境启动。
func (s *State) Ready() bool {
	s.mu.RLock()
	defer s.mu.RUnlock()
	for _, ok := range s.checks {
		if !ok {
			return false
		}
	}
	return true
}

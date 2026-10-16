// 本文件为Dedicated Server提供已认证的分配通知等待；在控制面提交真实Assignment时唤醒，不以轮询地图或UI驱动分配。
package gameservercontrol

import (
	"context"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"time"
)

// notifyAssignmentLocked必须在s.mu写锁内调用，关闭当前等待并移除，下一次订阅不会复用旧通道。
func (s *Service) notifyAssignmentLocked(id string) {
	if signal := s.assignmentChanged[id]; signal != nil {
		close(signal)
		delete(s.assignmentChanged, id)
	}
}

// WaitServerAssignment先核Boot，再原子检查/登记等待，避免提交在检查与订阅之间丢失；最多等待20秒便于传输恢复。
func (s *Service) WaitServerAssignment(ctx context.Context, id, boot string, wait time.Duration) (gameservercontract.ServerAssignment, bool, error) {
	if err := s.ValidateServerBoot(id, boot); err != nil {
		return gameservercontract.ServerAssignment{}, false, err
	}
	if wait < 0 || wait > 20*time.Second {
		wait = 20 * time.Second
	}
	s.mu.Lock()
	if current, found := s.assignments[id]; found {
		s.mu.Unlock()
		return cloneServerAssignment(current.assignment), true, nil
	}
	if s.assignmentChanged == nil {
		s.assignmentChanged = map[string]chan struct{}{}
	}
	signal := s.assignmentChanged[id]
	if signal == nil {
		signal = make(chan struct{})
		s.assignmentChanged[id] = signal
	}
	s.mu.Unlock()
	if wait > 0 {
		deadline := time.NewTimer(wait)
		defer deadline.Stop()
		select {
		case <-ctx.Done():
			return gameservercontract.ServerAssignment{}, false, ctx.Err()
		case <-deadline.C:
		case <-signal:
		}
	}
	if err := s.ValidateServerBoot(id, boot); err != nil {
		return gameservercontract.ServerAssignment{}, false, err
	}
	value, found := s.GetServerAssignment(id)
	return value, found, nil
}

package health

import "testing"

func TestStateBecomesReadyOnlyWhenAllChecksPass(t *testing.T) {
	state := NewState()
	state.Set("database", true)
	state.Set("cache", false)
	if state.Ready() {
		t.Fatal("存在失败检查时不应Ready")
	}
	state.Set("cache", true)
	if !state.Ready() {
		t.Fatal("全部检查通过时应Ready")
	}
}

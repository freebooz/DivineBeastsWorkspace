package apperror

import "testing"

func TestNewPreservesCodeAndRetryable(t *testing.T) {
	err := New("GAME_SERVER_NO_CAPACITY", "没有可用服务器容量", true)
	if err.Code != "GAME_SERVER_NO_CAPACITY" || !err.Retryable {
		t.Fatalf("应用错误字段不正确: %+v", err)
	}
}

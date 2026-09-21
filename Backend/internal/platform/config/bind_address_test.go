package config

import "testing"

// 防止联调显式回环配置被装配忽略；空值兼容旧行为，非法地址拒绝启动。
func TestServiceBindAddress(t *testing.T) {
	for _, address := range []string{"127.0.0.1", "::1", "0.0.0.0", ""} {
		t.Setenv("SERVICE_BIND_ADDRESS", address)
		cfg, err := LoadServiceConfig()
		if err != nil {
			t.Fatal(err)
		}
		if cfg.BindAddress != address {
			t.Fatalf("绑定地址丢失: got=%q want=%q", cfg.BindAddress, address)
		}
	}
	t.Setenv("SERVICE_BIND_ADDRESS", "127.0.0.1:8080")
	if _, err := LoadServiceConfig(); err == nil {
		t.Fatal("绑定地址不能夹带端口")
	}
}

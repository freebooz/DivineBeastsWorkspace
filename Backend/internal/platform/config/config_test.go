package config

import (
	"os"
	"testing"
	"time"
)

func TestLoadServiceConfigFromEnvironment(t *testing.T) {
	t.Setenv("SERVICE_NAME", "MatchService")
	t.Setenv("SERVICE_PORT", "18080")
	t.Setenv("SERVICE_VERSION", "0.1.0")
	t.Setenv("SHUTDOWN_TIMEOUT", "7s")
	cfg, err := LoadServiceConfig()
	if err != nil {
		t.Fatalf("加载配置失败: %v", err)
	}
	if cfg.Name != "MatchService" || cfg.Port != 18080 || cfg.Version != "0.1.0" || cfg.ShutdownTimeout != 7*time.Second {
		t.Fatalf("配置解析不正确: %+v", cfg)
	}
	_ = os.Unsetenv("SERVICE_NAME")
}

func TestLoadServiceConfigRejectsInvalidPort(t *testing.T) {
	t.Setenv("SERVICE_NAME", "GatewayService")
	t.Setenv("SERVICE_PORT", "70000")
	if _, err := LoadServiceConfig(); err == nil {
		t.Fatal("非法端口必须被拒绝")
	}
}

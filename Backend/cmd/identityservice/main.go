// Package main（程序入口）启动IdentityService。
package main

import (
	"context"
	"log/slog"
	"os"
	"os/signal"
	"syscall"

	"divinebeasts/backend/internal/app/composition"
	"divinebeasts/backend/internal/platform/config"
)

func main() {
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	cfg, err := config.LoadServiceConfig()
	if err != nil {
		slog.Error("加载服务配置失败", "error", err)
		os.Exit(1)
	}
	if os.Getenv("SERVICE_NAME") == "" {
		cfg.Name = "IdentityService"
	}
	if os.Getenv("SERVICE_PORT") == "" {
		cfg.Port = 8081
	}
	if os.Getenv("GRPC_PORT") == "" {
		cfg.GRPCPort = 9081
	}

	if err := composition.RunIdentity(ctx, cfg); err != nil {
		slog.Error("业务服务退出", "service", cfg.Name, "error", err)
		os.Exit(1)
	}
}

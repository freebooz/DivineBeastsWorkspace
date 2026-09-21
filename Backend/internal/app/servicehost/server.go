package servicehost

import (
	"context"
	"errors"
	"fmt"
	"log/slog"
	"net"
	"net/http"
	"strconv"
	"time"

	"divinebeasts/backend/internal/platform/config"
)

// Run（运行服务）启动HTTP运维端点并在Context取消时执行Graceful Shutdown（优雅停机）。
// 实际业务HTTP/gRPC路由可以通过businessHandler组合到同一个Mux中。
func Run(ctx context.Context, cfg config.ServiceConfig, businessHandler http.Handler) error {
	mux := http.NewServeMux()
	base := NewHandler(cfg.Name, cfg.Version)
	mux.Handle("/health/live", base)
	mux.Handle("/health/ready", base)
	mux.Handle("/version", base)
	if businessHandler != nil {
		mux.Handle("/", businessHandler)
	}

	server := &http.Server{
		Addr:              net.JoinHostPort(cfg.BindAddress, strconv.Itoa(cfg.Port)),
		Handler:           requestLoggingMiddleware(cfg.Name, mux),
		ReadHeaderTimeout: 5 * time.Second,
		ReadTimeout:       15 * time.Second,
		WriteTimeout:      15 * time.Second,
		IdleTimeout:       60 * time.Second,
	}

	errCh := make(chan error, 1)
	go func() {
		slog.Info("业务服务开始监听", "service", cfg.Name, "port", cfg.Port, "version", cfg.Version)
		errCh <- server.ListenAndServe()
	}()

	select {
	case <-ctx.Done():
		shutdownCtx, cancel := context.WithTimeout(context.Background(), cfg.ShutdownTimeout)
		defer cancel()
		if err := server.Shutdown(shutdownCtx); err != nil {
			return fmt.Errorf("服务优雅停机失败: %w", err)
		}
		return nil
	case err := <-errCh:
		if errors.Is(err, http.ErrServerClosed) {
			return nil
		}
		return err
	}
}

// requestLoggingMiddleware（请求日志中间件）记录最小HTTP访问日志。
// 生产环境应在Envoy/Gateway层同时传递RequestId和TraceId。
func requestLoggingMiddleware(serviceName string, next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()
		next.ServeHTTP(w, r)
		slog.Info("HTTP请求完成", "service", serviceName, "method", r.Method, "path", r.URL.Path, "duration", time.Since(start))
	})
}

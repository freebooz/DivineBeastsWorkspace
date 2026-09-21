//go:build productiondeps

// Package postgres（PostgreSQL生产适配器）使用pgxpool提供高并发连接池。
package postgres

import (
	"context"
	"fmt"
	"time"

	"github.com/jackc/pgx/v5/pgxpool"
)

// Config（PostgreSQL连接池配置）集中控制最大连接数和连接健康参数。
type Config struct {
	DSN          string        // DSN（数据库连接字符串）。
	MaxConns     int32         // MaxConns（最大连接数）。
	MinIdleConns int32         // MinIdleConns（最小空闲连接数），用于降低突发流量的尾延迟。
	PingTimeout  time.Duration // PingTimeout（首次连通性检查超时）。
}

// Pool（PostgreSQL连接池）封装pgxpool，业务领域只依赖Repository接口，不直接依赖本类型。
type Pool struct{ inner *pgxpool.Pool }

// Open（打开连接池）创建连接池并立即执行Ping，避免服务启动成功后才发现数据库不可达。
func Open(ctx context.Context, cfg Config) (*Pool, error) {
	parsed, err := pgxpool.ParseConfig(cfg.DSN)
	if err != nil {
		return nil, fmt.Errorf("解析PostgreSQL配置失败: %w", err)
	}
	if cfg.MaxConns > 0 {
		parsed.MaxConns = cfg.MaxConns
	}
	if cfg.MinIdleConns > 0 {
		parsed.MinIdleConns = cfg.MinIdleConns
	}
	pool, err := pgxpool.NewWithConfig(ctx, parsed)
	if err != nil {
		return nil, fmt.Errorf("创建PostgreSQL连接池失败: %w", err)
	}
	pingCtx := ctx
	var cancel context.CancelFunc
	if cfg.PingTimeout > 0 {
		pingCtx, cancel = context.WithTimeout(ctx, cfg.PingTimeout)
		defer cancel()
	}
	if err := pool.Ping(pingCtx); err != nil {
		pool.Close()
		return nil, fmt.Errorf("PostgreSQL Ping失败: %w", err)
	}
	return &Pool{inner: pool}, nil
}

// Inner（获取底层连接池）供sqlc生成的Queries绑定使用。
func (p *Pool) Inner() *pgxpool.Pool { return p.inner }

// Ping（健康检查）验证数据库连接池当前可用。
func (p *Pool) Ping(ctx context.Context) error { return p.inner.Ping(ctx) }

// Close（关闭连接池）在服务优雅停机时释放数据库连接。
func (p *Pool) Close() { p.inner.Close() }

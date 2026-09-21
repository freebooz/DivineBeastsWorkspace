//go:build productiondeps

// Package redisstore（Redis生产适配器）统一支持单节点、Sentinel和Redis Cluster连接方式。
package redisstore

import (
	"context"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
)

// Config（Redis配置）描述高性能热状态存储连接参数。
type Config struct {
	Addrs        []string      // Addrs（Redis地址列表）；多个地址时可连接Cluster。
	Password     string        // Password（Redis密码）。
	MasterName   string        // MasterName（Sentinel主节点名称），不用Sentinel时留空。
	DB           int           // DB（逻辑数据库），Cluster模式固定使用0。
	PoolSize     int           // PoolSize（连接池大小）。
	MinIdleConns int           // MinIdleConns（最小空闲连接数）。
	DialTimeout  time.Duration // DialTimeout（连接超时）。
}

// Client（Redis客户端）封装UniversalClient，使本地单节点与生产Cluster使用同一代码路径。
type Client struct{ inner redis.UniversalClient }

// Open（打开Redis客户端）创建连接并执行Ping验证。
func Open(ctx context.Context, cfg Config) (*Client, error) {
	client := redis.NewUniversalClient(&redis.UniversalOptions{
		Addrs:        cfg.Addrs,
		Password:     cfg.Password,
		MasterName:   cfg.MasterName,
		DB:           cfg.DB,
		PoolSize:     cfg.PoolSize,
		MinIdleConns: cfg.MinIdleConns,
		DialTimeout:  cfg.DialTimeout,
	})
	if err := client.Ping(ctx).Err(); err != nil {
		_ = client.Close()
		return nil, fmt.Errorf("Redis Ping失败: %w", err)
	}
	return &Client{inner: client}, nil
}

// Inner（获取底层客户端）供具体Repository实现Redis命令。
func (c *Client) Inner() redis.UniversalClient { return c.inner }

// Ping（健康检查）验证Redis当前可用。
func (c *Client) Ping(ctx context.Context) error { return c.inner.Ping(ctx).Err() }

// Close（关闭Redis客户端）释放底层连接池。
func (c *Client) Close() error { return c.inner.Close() }

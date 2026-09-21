//go:build productiondeps

// Package natsjs（NATS JetStream生产适配器）负责跨Application Integration Event（集成事件）持久发布。
package natsjs

import (
	"context"
	"fmt"
	"time"

	"github.com/nats-io/nats.go"
	"github.com/nats-io/nats.go/jetstream"
)

// Config（NATS连接配置）描述消息系统连接与重连策略。
type Config struct {
	URL            string        // URL（NATS服务器地址）。
	ClientName     string        // ClientName（连接名称），便于服务端监控识别。
	ConnectTimeout time.Duration // ConnectTimeout（首次连接超时）。
	ReconnectWait  time.Duration // ReconnectWait（断线重连间隔）。
}

// Publisher（JetStream事件发布器）提供带MessageID去重的持久化发布能力。
type Publisher struct {
	conn *nats.Conn
	js   jetstream.JetStream
}

// Open（连接NATS JetStream）创建连接与JetStream上下文。
func Open(cfg Config) (*Publisher, error) {
	conn, err := nats.Connect(
		cfg.URL,
		nats.Name(cfg.ClientName),
		nats.Timeout(cfg.ConnectTimeout),
		nats.MaxReconnects(-1),
		nats.ReconnectWait(cfg.ReconnectWait),
	)
	if err != nil {
		return nil, fmt.Errorf("连接NATS失败: %w", err)
	}
	js, err := jetstream.New(conn)
	if err != nil {
		conn.Close()
		return nil, fmt.Errorf("创建JetStream上下文失败: %w", err)
	}
	return &Publisher{conn: conn, js: js}, nil
}

// Publish（发布集成事件）使用MessageID启用JetStream服务器端消息去重。
func (p *Publisher) Publish(ctx context.Context, subject, messageID string, payload []byte) error {
	_, err := p.js.Publish(ctx, subject, payload, jetstream.WithMsgID(messageID))
	if err != nil {
		return fmt.Errorf("发布JetStream事件失败: %w", err)
	}
	return nil
}

// Close（关闭消息连接）先Flush已发送数据，再关闭NATS连接。
func (p *Publisher) Close() error {
	if err := p.conn.Flush(); err != nil {
		p.conn.Close()
		return err
	}
	p.conn.Close()
	return nil
}

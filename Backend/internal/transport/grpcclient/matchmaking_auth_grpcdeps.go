//go:build grpcdeps

// Match连接包装器把部署凭据仅放入metadata；所有旧Party与新匹配端点共享认证，禁止重定向或记录秘密。
package grpcclient

import (
	"context"
	"google.golang.org/grpc"
	"google.golang.org/grpc/metadata"
)

type authenticatedMatchConn struct {
	grpc.ClientConnInterface
	token string
}

func (c *authenticatedMatchConn) Invoke(ctx context.Context, method string, args, reply any, opts ...grpc.CallOption) error {
	return c.ClientConnInterface.Invoke(metadata.AppendToOutgoingContext(ctx, "authorization", "Bearer "+c.token), method, args, reply, opts...)
}

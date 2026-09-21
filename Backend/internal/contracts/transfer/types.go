// Package transfer（服务器迁移共享契约适配）定义Go业务层稳定使用的跨服迁移DTO。
package transfer

import "time"

// IssueRequest（迁移票据签发请求）描述玩家迁往目标Dedicated Server的必要上下文。
type IssueRequest struct {
	GameID                  string        // GameID（游戏ID）。
	PlayerID                string        // PlayerID（玩家ID）。
	SessionID               string        // SessionID（玩家在线会话ID）。
	SourceGameServerID      string        // SourceGameServerID（来源服务器ID，首次进服可为空）。
	DestinationGameServerID string        // DestinationGameServerID（目标服务器ID）。
	DestinationWorldID      string        // DestinationWorldID（目标世界ID）。
	MatchID                 string        // MatchID（目标比赛ID，非竞技迁移可为空）。
	TTL                     time.Duration // TTL（票据最大有效时间）。
}

// Ticket（服务器迁移票据）是ClientTravel前由Backend签发的短期一次性凭据。
type Ticket struct {
	TicketID        string    // TicketID（迁移票据唯一ID）。
	DestinationHost string    // DestinationHost（目标服务器主机地址）。
	DestinationPort uint32    // DestinationPort（目标服务器端口）。
	ExpiresAt       time.Time // ExpiresAt（票据失效时间）。
	Nonce           string    // Nonce（防重放随机数）。
	Signature       string    // Signature（Backend数字签名/HMAC）。
}

// Validation（迁移票据验证结果）向目标Dedicated Server返回可信玩家上下文。
type Validation struct {
	Valid     bool   // Valid（票据是否验证通过）。
	PlayerID  string // PlayerID（已验证玩家ID）。
	SessionID string // SessionID（已验证在线会话ID）。
	MatchID   string // MatchID（目标竞技比赛ID）。
	ErrorCode string // ErrorCode（标准错误码，成功时为空）。
}

// Package apperror（应用错误包）定义跨Application统一使用的业务错误结构。
package apperror

import "fmt"

// Error（应用错误）表示可以稳定映射到Shared Contract错误码的业务错误。
type Error struct {
	Code      string // Code（错误码）必须是稳定、可机器识别的字符串。
	Message   string // Message（错误消息）用于日志和开发诊断，不作为客户端逻辑判断依据。
	Retryable bool   // Retryable（是否可重试）表示调用方是否可以安全重试当前操作。
}

// New（创建应用错误）构造统一业务错误。
func New(code, message string, retryable bool) *Error {
	return &Error{Code: code, Message: message, Retryable: retryable}
}

// Error（实现error接口）返回带错误码的可读字符串。
func (e *Error) Error() string {
	return fmt.Sprintf("%s: %s", e.Code, e.Message)
}

// Package httpadapter（HTTP传输适配器）提供Backend服务间HTTP/JSON协议和本地联调客户端。
// 正式生产可切换到grpcdeps构建标签使用gRPC，应用/领域层无需修改。
package httpadapter

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"divinebeasts/backend/internal/app/gateway"
	"divinebeasts/backend/internal/modules/identity"
	"divinebeasts/backend/internal/platform/apperror"
)

const maxBodyBytes = 1 << 20

func decodeJSON(r *http.Request, value any) error {
	decoder := json.NewDecoder(io.LimitReader(r.Body, maxBodyBytes))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(value); err != nil {
		return fmt.Errorf("请求JSON无效: %w", err)
	}
	return nil
}

func writeJSON(w http.ResponseWriter, status int, value any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(value)
}

func writeError(w http.ResponseWriter, status int, code string, err error) {
	message := code
	if err != nil {
		message = err.Error()
	}
	writeJSON(w, status, map[string]any{"errorCode": code, "message": message})
}

func statusForError(err error) int {
	if err == nil {
		return http.StatusOK
	}
	if errors.Is(err, contextCanceled) {
		return http.StatusRequestTimeout
	}
	return http.StatusBadRequest
}

var contextCanceled = errors.New("context canceled")

// onlineDomainError 只映射可证明的领域错误；未知故障503，不泄漏底层描述。
func onlineDomainError(err error) error {
 if err==nil {return nil}
 var app *apperror.Error
 if errors.As(err,&app) {return gateway.ServiceError(app.Code)}
 switch {
 case errors.Is(err,identity.ErrInvalidCredentials):return gateway.ServiceError("AUTH_INVALID_CREDENTIALS")
 case errors.Is(err,identity.ErrInvalidToken):return gateway.ServiceError("AUTH_SESSION_INVALID")
 case errors.Is(err,identity.ErrTokenExpired):return gateway.ServiceError("AUTH_TOKEN_EXPIRED")
 case errors.Is(err,identity.ErrInvalidInput):return gateway.ServiceError("INVALID_REQUEST")
 default:return gateway.ServiceError("SERVICE_UNAVAILABLE")
 }
}
func writeOnlineDomainError(w http.ResponseWriter,err error) {status,code:=gateway.OnlineErrorStatus(onlineDomainError(err));writeError(w,status,code,nil)}

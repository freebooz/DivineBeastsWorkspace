// Package httpadapter（HTTP传输适配器）提供Backend服务间HTTP/JSON协议和本地联调客户端。
// 正式生产可切换到grpcdeps构建标签使用gRPC，应用/领域层无需修改。
package httpadapter

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
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

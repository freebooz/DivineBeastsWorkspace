// Package servicehost（服务宿主）提供所有Go业务服务统一的健康检查、版本接口和HTTP宿主基础能力。
package servicehost

import (
	"encoding/json"
	"net/http"
)

// Handler（基础HTTP处理器）提供Liveness（存活）、Readiness（就绪）和Version（版本）端点。
type Handler struct {
	serviceName string
	version     string
}

// NewHandler（创建基础处理器）创建一个无需外部依赖即可工作的健康接口处理器。
func NewHandler(serviceName, version string) http.Handler {
	return &Handler{serviceName: serviceName, version: version}
}

// ServeHTTP（处理HTTP请求）实现基础运维端点。
func (h *Handler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	switch r.URL.Path {
	case "/health/live":
		writeJSON(w, http.StatusOK, map[string]any{"status": "ok", "service": h.serviceName})
	case "/health/ready":
		writeJSON(w, http.StatusOK, map[string]any{"status": "ready", "service": h.serviceName})
	case "/version":
		writeJSON(w, http.StatusOK, map[string]any{"service": h.serviceName, "version": h.version})
	default:
		writeJSON(w, http.StatusNotFound, map[string]any{"errorCode": "NOT_FOUND", "message": "接口不存在"})
	}
}

func writeJSON(w http.ResponseWriter, status int, value any) {
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(value)
}

// Package config（配置包）集中管理Go业务服务的环境变量读取和基础校验。
package config

import (
	"errors"
	"net"
	"os"
	"strconv"
	"strings"
	"time"
)

// ServiceConfig（服务配置）描述所有Go业务服务共享的启动参数。
type ServiceConfig struct {
	BindAddress string // SERVICE_BIND_ADDRESS只包含IP；空值保持旧监听语义，联调显式127.0.0.1。
	Name            string        // Name（服务名称）。
	Port            int           // Port（HTTP监听端口）。
	GRPCPort        int           // GRPCPort（gRPC监听端口；grpcdeps构建使用）。
	Version         string        // Version（构建版本）。
	ShutdownTimeout time.Duration // ShutdownTimeout（优雅停机超时）。
}

// LoadServiceConfig（加载服务配置）从环境变量读取统一启动参数。
func LoadServiceConfig() (ServiceConfig, error) {
	bindAddress:=os.Getenv("SERVICE_BIND_ADDRESS")
	if bindAddress!="" && net.ParseIP(bindAddress)==nil {return ServiceConfig{},errors.New("SERVICE_BIND_ADDRESS必须是IP地址且不能包含端口")}
	name := getenv("SERVICE_NAME", "UnknownService")
	version := getenv("SERVICE_VERSION", "1.1.0")
	port, err := envPort("SERVICE_PORT", 8080)
	if err != nil {
		return ServiceConfig{}, err
	}
	grpcPort, err := envPort("GRPC_PORT", port+1000)
	if err != nil {
		return ServiceConfig{}, err
	}
	shutdownText := getenv("SHUTDOWN_TIMEOUT", "10s")
	shutdownTimeout, err := time.ParseDuration(shutdownText)
	if err != nil || shutdownTimeout <= 0 {
		return ServiceConfig{}, errors.New("SHUTDOWN_TIMEOUT必须是有效的正Duration")
	}
	return ServiceConfig{Name: name, Port: port, GRPCPort: grpcPort, Version: version, ShutdownTimeout: shutdownTimeout,BindAddress:bindAddress}, nil
}

func envPort(key string, fallback int) (int, error) {
	text := os.Getenv(key)
	if text == "" {
		return fallback, nil
	}
	value, err := strconv.Atoi(text)
	if err != nil || value < 1 || value > 65535 {
		return 0, errors.New(key + "必须是1到65535之间的整数")
	}
	return value, nil
}

// Getenv（读取环境变量）供Composition读取非敏感运行参数；为空时返回fallback。
func Getenv(key, fallback string) string { return getenv(key, fallback) }

// GetenvDuration（读取Duration环境变量）解析失败时返回错误。
func GetenvDuration(key string, fallback time.Duration) (time.Duration, error) {
	text := os.Getenv(key)
	if text == "" {
		return fallback, nil
	}
	value, err := time.ParseDuration(text)
	if err != nil || value <= 0 {
		return 0, errors.New(key + "必须是有效的正Duration")
	}
	return value, nil
}

// GetenvCSV（读取逗号分隔环境变量）用于Redis Cluster/Sentinel地址列表。
func GetenvCSV(key string) []string {
	raw := strings.TrimSpace(os.Getenv(key))
	if raw == "" {
		return nil
	}
	parts := strings.Split(raw, ",")
	result := make([]string, 0, len(parts))
	for _, part := range parts {
		if value := strings.TrimSpace(part); value != "" {
			result = append(result, value)
		}
	}
	return result
}

func getenv(key, fallback string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return fallback
}

# 五个后端薄入口统一使用锁定Go工具链；本地镜像只供开发联调，生产依赖由独立构建配置显式装配。
FROM golang:1.23.12 AS builder

WORKDIR /workspace/Backend

COPY Backend/go.mod Backend/go.sum ./
RUN go mod download

COPY Backend/ ./

ARG SERVICE
RUN test -n "$SERVICE" && \
    CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -trimpath -ldflags="-s -w" -o /output/service "./cmd/${SERVICE}"

FROM scratch

COPY --from=builder /output/service /service

USER 65532:65532

ENTRYPOINT ["/service"]

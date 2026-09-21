FROM golang:1.23 AS builder

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

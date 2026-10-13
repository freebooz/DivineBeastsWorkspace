#!/usr/bin/env python3
# Agones Pod 专用服务器启动入口。
# 职责：向本 Pod 的 127.0.0.1 Sidecar 读取 GameServer 名称、动态对外端口及地址，
# 校验启动角色和部署环境，再用 exec 替换为 UE Dedicated Server 进程（保留原样退出码与 SIGTERM）。
# 不进行玩家分配、不会调用 Agones Ready/Allocate，也不存储敏感令牌。
# 容器镜像必须预装 Python 3，并包含实际可执行的 UE Linux Server 包及三角色 Profile。
from __future__ import annotations

import ipaddress
import json
import os
from pathlib import Path
import sys
import time
from urllib.error import HTTPError, URLError
from urllib.request import urlopen

ROLES = {"OpenWorld", "Village", "MainArena"}
GAME_PORT_NAME = "game"


def positive_int(value: str, field: str, maximum: int = 65535) -> int:
    """校验十进制正整数，避免空值、布尔字符串和越界端口被静默接受。"""
    if not value or not value.isascii() or not value.isdecimal():
        raise ValueError(f"{field} 必须是正整数")
    number = int(value)
    if not 1 <= number <= maximum:
        raise ValueError(f"{field} 超出允许范围")
    return number


def canonical_endpoint(host: str, port: int) -> str:
    """仅构造客户端连接的主机和端口；拒绝用户信息、URL、路径及控制字符。"""
    if not host or len(host) > 253 or any(c.isspace() for c in host):
        raise ValueError("Agones 返回的主机地址无效")
    if "://" in host or "/" in host or "@" in host or "?" in host or "#" in host:
        raise ValueError("Agones 返回的主机地址不是主机标识")
    try:
        parsed = ipaddress.ip_address(host)
    except ValueError:
        # Kubernetes 节点 DNS 名称：保留 ASCII 字母、数字、点和连字符。
        if not host.isascii() or any(not (c.isalnum() or c in ".-") for c in host):
            raise ValueError("Agones 返回的节点主机名无效")
        return f"{host}:{port}"
    return f"[{parsed}]:{port}" if parsed.version == 6 else f"{parsed}:{port}"


def game_server_identity(payload: dict, fallback_host: str) -> tuple[str, str]:
    """从 Agones SDK 的权威 GameServer 视图取唯一名称及 UDP 动态端点。"""
    metadata = payload.get("object_meta") or {}
    status = payload.get("status") or {}
    server_id = metadata.get("name")
    if not isinstance(server_id, str) or not server_id or len(server_id) > 63:
        raise ValueError("GameServer 名称缺失")
    if any(not (ch.isascii() and (ch.isalnum() or ch in "-.")) for ch in server_id):
        raise ValueError("GameServer 名称非法")
    ports = [
        entry.get("port")
        for entry in status.get("ports", [])
        if isinstance(entry, dict) and entry.get("name") == GAME_PORT_NAME
    ]
    if len(ports) != 1:
        raise ValueError("Agones 尚未提供唯一 game 端口")
    port = positive_int(str(ports[0]), "Agones game port")
    # SDK 主地址优先；某些版本在 Ready 前只给 NodeIP 时可使用 Pod 的真实宿主 IP。
    address = status.get("address") or fallback_host
    if not isinstance(address, str):
        raise ValueError("Agones 地址无效")
    return server_id, canonical_endpoint(address, port)


def discover_game_server(http_port: int, fallback_host: str, timeout_seconds: float = 90.0):
    """Sidecar 可能比容器启动晚，有限重试直到地址及端口真实可用；超时失败关闭。"""
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        try:
            with urlopen(f"http://127.0.0.1:{http_port}/gameserver", timeout=2) as response:
                if not 200 <= response.status < 300:
                    raise ValueError("Agones SDK 返回非成功状态")
                document = json.loads(response.read(65536))
            if not isinstance(document, dict):
                raise ValueError("GameServer 数据结构非法")
            return game_server_identity(document, fallback_host)
        except (OSError, HTTPError, URLError, ValueError, json.JSONDecodeError):
            time.sleep(2)
    raise TimeoutError("Agones SDK 未提供有效 GameServer 身份及连接端点")


def build_launch_environment(source: dict, server_id: str, endpoint: str) -> tuple[dict, list[str], str]:
    """检查角色、真实包和 Profile，不填充假身份；返回 exec 所需完整环境和参数。"""
    role = source.get("GAME_SERVER_ROLE", "")
    if role not in ROLES:
        raise ValueError("GAME_SERVER_ROLE 必须为 OpenWorld、Village 或 MainArena")
    base = Path(source.get("GAME_SERVER_WORKDIR", "/opt/sslm"))
    profile_path = base / "Deploy" / "Server" / role / "server-profile.json"
    with profile_path.open(encoding="utf-8") as profile_file:
        profile = json.load(profile_file)
    world_package = profile.get("worldPackage")
    if not isinstance(world_package, str) or not world_package.startswith("/"):
        raise ValueError("角色 Profile 缺少合法的 worldPackage")
    capacity = positive_int(source.get("GAME_SERVER_CAPACITY", ""), "GAME_SERVER_CAPACITY", 10000)
    protocol = positive_int(source.get("GAME_SERVER_PROTOCOL_VERSION", ""), "GAME_SERVER_PROTOCOL_VERSION", 100000)
    if not source.get("GAME_SERVER_REGION_ID") or not source.get("GAME_SERVER_BUILD_VERSION"):
        raise ValueError("服务器区域或构建版本未配置")
    if not source.get("GAMESERVERCONTROL_BASE_URL", "").startswith("https://"):
        raise ValueError("正式部署必须设置 HTTPS 的 GAMESERVERCONTROL_BASE_URL")
    if not source.get("GAMESERVERCONTROL_INTERNAL_TOKEN"):
        raise ValueError("服务器控制面内部令牌未注入")
    executable = source.get("GAME_SERVER_EXECUTABLE", "")
    if not executable or not Path(executable).is_file() or not os.access(executable, os.X_OK):
        raise ValueError("尚未部署可执行的 UE Dedicated Server Linux 程序")
    env = dict(source)
    env["GAME_SERVER_ID"] = server_id
    env["GAME_SERVER_WORLD_ID"] = env.get("GAME_SERVER_WORLD_ID") or server_id
    env["GAME_SERVER_PUBLIC_ENDPOINT"] = endpoint
    env["GAME_SERVER_CAPACITY"] = str(capacity)
    env["GAME_SERVER_PROTOCOL_VERSION"] = str(protocol)
    # 同一容器重新启动进程不能复用旧启动代次，避免过期准入重新生效。
    env.pop("GAME_SERVER_BOOT_ID", None)
    # 运行 UE 服务器：当前地图由经校验的 Profile 提供，不创建额外服务器目标。
    argv = [
        executable,
        world_package,
        "-log",
        "-Port=7777",
        f"-ServerRole={role}",
    ]
    return env, argv, str(base)


def main() -> int:
    """容器启动失败直接非零退出；不隐瞒读取身份/资源失败，不启动半配置的服务器。"""
    try:
        http_port = positive_int(os.environ.get("AGONES_SDK_HTTP_PORT", ""), "AGONES_SDK_HTTP_PORT")
        server_id, endpoint = discover_game_server(
            http_port, os.environ.get("GAME_SERVER_NODE_IP", "")
        )
        env, argv, cwd = build_launch_environment(os.environ, server_id, endpoint)
        os.chdir(cwd)
        os.execvpe(argv[0], argv, env)
    except (OSError, ValueError, TimeoutError, json.JSONDecodeError) as exc:
        # 错误中仅包含可公开的配置类型，不输出 env、令牌或用户数据。
        print(f"Agones 游戏服务器启动失败：{type(exc).__name__}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

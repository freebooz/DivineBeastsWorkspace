#!/usr/bin/env python3
# 跨系统部署合同测试：只读检查 Agones 模板与启动器纯函数。
# 无 Agones/UE/Kubernetes 环境时只证明语法及输入校验，不能冒充真实 Fleet 扩缩验收。
import importlib.util
from io import BytesIO
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch
from urllib.request import ProxyHandler

ROOT = Path(__file__).resolve().parents[2]
LAUNCHER = ROOT / "Deploy" / "Agones" / "launch_game_server.py"
spec = importlib.util.spec_from_file_location("sslm_agones_launcher", LAUNCHER)
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


class AgonesBootstrapContractTests(unittest.TestCase):
    """启动器的身份、动态端口、角色与凭据失败关闭合同。"""

    def test_dynamic_host_port_and_gameserver_identity(self):
        """Agones 真正分配的 HostPort 必须用于对外端点，而非 UE 内部的 7777。"""
        source = {
            "object_meta": {"name": "sslm-main-arena-abc123"},
            "status": {
                "address": "192.0.2.10",
                "ports": [{"name": "game", "port": 32221}],
            },
        }
        self.assertEqual(
            launcher.game_server_identity(source, ""),
            ("sslm-main-arena-abc123", "192.0.2.10:32221"),
        )

    def test_missing_or_duplicate_port_fails_closed(self):
        """端口缺失或重复时绝不回退为固定 7777，避免把玩家送到错误实例。"""
        for ports in [[], [{"name": "http", "port": 7777}], [
            {"name": "game", "port": 30000}, {"name": "game", "port": 30001}
        ]]:
            with self.subTest(ports=ports):
                with self.assertRaises(ValueError):
                    launcher.game_server_identity({
                        "object_meta": {"name": "server-a"},
                        "status": {"address": "192.0.2.20", "ports": ports},
                    }, "")

    def test_invalid_port_and_endpoint(self):
        """拒绝端口注入、控制字符、无效网络标识以及无界数值。"""
        for value in ("", "0", "65536", "9358;rm -rf", "-1", "5.5"):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    launcher.positive_int(value, "port")
        with self.assertRaises(ValueError):
            launcher.canonical_endpoint("http://attacker.test/", 30000)
        self.assertEqual(
            launcher.canonical_endpoint("2001:db8::1", 30000),
            "[2001:db8::1]:30000",
        )

    def test_reject_unroutable_and_malformed_public_endpoints(self):
        """控制面绝不能发布回环、未指定、组播或非法 DNS/端口作为对外连接端点。"""
        for host in ("127.0.0.1", "::1", "0.0.0.0", "224.0.0.1",
                     "a..b", "-bad.example", "bad-.example",
                     "localhost", "a.localhost", "999.999.999.999"):
            with self.subTest(host=host), self.assertRaises(ValueError):
                launcher.canonical_endpoint(host, 31000)
        for port in (0, 65536, -1, True, "31000"):
            with self.subTest(port=port), self.assertRaises(ValueError):
                launcher.canonical_endpoint("gameserver-1.internal.example", port)
        self.assertEqual(
            launcher.canonical_endpoint("gameserver-1.internal.example", 31000),
            "gameserver-1.internal.example:31000",
        )

    def test_reject_invalid_gameserver_resource_names(self):
        """无效 Kubernetes 名称不能进入 Go 注册表或作为运行实例 ID。"""
        for name in ("BadUpper", "-bad", "bad-", "a..b", "name_", "."):
            with self.subTest(name=name), self.assertRaises(ValueError):
                launcher.game_server_identity({
                    "object_meta": {"name": name},
                    "status": {"address": "192.0.2.2",
                               "ports": [{"name": "game", "port": 31000}]},
                }, "")

    def test_reject_malformed_sdk_response_shapes(self):
        """Sidecar 返回非对象或伪造状态结构时以 ValueError 失败关闭。"""
        for payload in (None, [], {"object_meta": "wrong", "status": {}},
                        {"object_meta": {"name": "server-1"}, "status": "wrong"},
                        {"object_meta": {"name": "server-1"},
                         "status": {"ports": "wrong", "address": "192.0.2.12"}}):
            with self.subTest(payload=payload), self.assertRaises(ValueError):
                launcher.game_server_identity(payload, "")

    def test_local_sdk_request_ignores_proxies_and_redirects(self):
        """启动前获取身份只允许走 loopback，拒绝环境代理与重定向。"""
        proxy_handlers = [
            h for h in launcher.SDK_HTTP_OPENER.handlers if isinstance(h, ProxyHandler)
        ]
        # urllib 的 build_opener 会忽略空 ProxyHandler；没有任何代理处理器才表明
        # 请求不继承 HTTP_PROXY/HTTPS_PROXY，此处不应断言保留空处理器实例。
        self.assertEqual(proxy_handlers, [])
        redirect_handlers = [
            h for h in launcher.SDK_HTTP_OPENER.handlers
            if isinstance(h, launcher.RejectSidecarRedirect)
        ]
        self.assertEqual(len(redirect_handlers), 1)
        self.assertIsNone(redirect_handlers[0].redirect_request(
            None, None, 302, "Found", {}, "http://not-a-sidecar.invalid/"
        ))

        payload = {
            "object_meta": {"name": "server-1"},
            "status": {"address": "192.0.2.12",
                       "ports": [{"name": "game", "port": 31234}]},
        }
        result = BytesIO(json.dumps(payload).encode("utf-8"))
        result.status = 200
        with patch.object(launcher.SDK_HTTP_OPENER, "open", return_value=result) as opener:
            self.assertEqual(
                launcher.discover_game_server(9358, "", timeout_seconds=5),
                ("server-1", "192.0.2.12:31234"),
            )
            opener.assert_called_once_with("http://127.0.0.1:9358/gameserver", timeout=2)

    def test_valid_profile_constructs_only_one_server_target(self):
        """三角色共享同一 Server 程序；地图和启动身份来自受控 Profile 与 Agones。"""
        with TemporaryDirectory() as tmp:
            base = Path(tmp)
            profile_dir = base / "Deploy" / "Server" / "MainArena"
            profile_dir.mkdir(parents=True)
            (profile_dir / "server-profile.json").write_text(
                json.dumps({
                    "serverRoleId": "GameServer.Role.MainArena",
                    "worldPackage": "/Game/Test/L_Arena",
                }), encoding="utf-8"
            )
            binary = base / "ServerBin"
            binary.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            binary.chmod(0o755)
            env = {
                "GAME_SERVER_ROLE": "MainArena",
                "GAME_SERVER_WORKDIR": str(base),
                "GAME_SERVER_CAPACITY": "10",
                "GAME_SERVER_REGION_ID": "example-region",
                "GAME_SERVER_BUILD_VERSION": "1.0.0",
                "GAME_SERVER_PROTOCOL_VERSION": "1",
                "GAME_SERVER_EXECUTABLE": str(binary),
                "GAMESERVERCONTROL_BASE_URL": "https://control.example.invalid",
                "GAMESERVERCONTROL_INTERNAL_TOKEN": "test-only-token",
                "GAME_SERVER_BOOT_ID": "stale-boot-id",
            }
            result, argv, working_dir = launcher.build_launch_environment(
                env, "arena-123", "192.0.2.21:31111"
            )
            self.assertEqual(result["GAME_SERVER_ID"], "arena-123")
            self.assertEqual(result["GAME_SERVER_PUBLIC_ENDPOINT"], "192.0.2.21:31111")
            self.assertEqual(result["GAME_SERVER_WORLD_ID"], "arena-123")
            self.assertNotIn("GAME_SERVER_BOOT_ID", result)
            self.assertEqual(argv[1], "/Game/Test/L_Arena")
            self.assertIn("-ServerRole=MainArena", argv)
            self.assertIn("-Port=7777", argv)
            self.assertEqual(working_dir, str(base))
            env["GAME_SERVER_ROLE"] = "Lobby"
            with self.assertRaises(ValueError):
                launcher.build_launch_environment(env, "bad", "192.0.2.21:31111")
            env["GAME_SERVER_ROLE"] = "MainArena"
            env["GAMESERVERCONTROL_BASE_URL"] = "http://insecure.example.invalid"
            with self.assertRaises(ValueError):
                launcher.build_launch_environment(env, "bad", "192.0.2.21:31111")
            # 即使角色字段合法，Profile 内容被错误映射也必须失败关闭。
            env["GAMESERVERCONTROL_BASE_URL"] = "https://control.example.invalid"
            (profile_dir / "server-profile.json").write_text(
                json.dumps({
                    "serverRoleId": "GameServer.Role.Village",
                    "worldPackage": "/Game/Test/L_Arena",
                }), encoding="utf-8"
            )
            with self.assertRaises(ValueError):
                launcher.build_launch_environment(env, "arena-123", "192.0.2.21:31111")


class AgonesManifestTests(unittest.TestCase):
    """三角色标签必须与 Go AgonesAllocator 选择器一致；模板为未替换镜像的示例。"""

    @classmethod
    def setUpClass(cls):
        try:
            import yaml
        except ImportError:
            cls.yaml = None
        else:
            cls.yaml = yaml

    def test_role_fleets_and_autoscaler_contract(self):
        """需要 PyYAML 执行 YAML 语义校验；没有依赖时明确跳过而不是声称通过。"""
        if self.yaml is None:
            self.skipTest("需要 PyYAML 执行 Agones CRD YAML 语义校验")
        expected = {
            "main-arena": ("main-arena", "Experience.MainArena.Main", "Buffer", 10),
            "open-world": ("open-world", "Experience.OpenWorld.Hub", "Counter", 40),
            "village": ("village", "Experience.Village.Tutorial", "Counter", 20),
        }
        with (ROOT / "Deploy" / "Agones" / "autoscalers.yaml").open(encoding="utf-8") as source:
            scalers = {
                entry["spec"]["fleetName"]: entry
                for entry in self.yaml.safe_load_all(source)
            }
        for filename, (label, experience, mode, capacity) in expected.items():
            with self.subTest(role=filename):
                with (ROOT / "Deploy" / "Agones" / f"fleet-{filename}.yaml").open(
                    encoding="utf-8"
                ) as source:
                    fleet = self.yaml.safe_load(source)
                self.assertEqual(fleet["kind"], "Fleet")
                labels = fleet["spec"]["template"]["metadata"]["labels"]
                self.assertEqual(labels["server-role"], label)
                self.assertEqual(labels["experience"], experience)
                ports = fleet["spec"]["template"]["spec"]["ports"]
                self.assertEqual(ports[0]["portPolicy"], "Dynamic")
                self.assertEqual(ports[0]["containerPort"], 7777)
                self.assertEqual(ports[0]["protocol"], "UDP")

                # 标签决定 Go 原子分配目标；进程变量、Profile 必须使用完全一致的身份。
                server_role = {"main-arena": "MainArena",
                               "open-world": "OpenWorld", "village": "Village"}[filename]
                profile_path = ROOT / "Deploy" / "Server" / server_role / "server-profile.json"
                profile = json.loads(profile_path.read_text(encoding="utf-8"))
                self.assertEqual(profile["serverRoleId"], f"GameServer.Role.{server_role}")
                self.assertEqual(profile["defaultExperienceId"], labels["experience"])
                container = fleet["spec"]["template"]["spec"]["template"]["spec"]["containers"][0]
                envs = {entry["name"]: entry for entry in container["env"]}
                self.assertEqual(envs["GAME_SERVER_ROLE"]["value"], server_role)
                self.assertEqual(envs["GAME_SERVER_CAPACITY"]["value"], str(capacity))
                self.assertEqual(envs["GAME_SERVER_REGION_ID"]["value"], labels["region"])
                self.assertEqual(envs["GAME_SERVER_BUILD_VERSION"]["value"], labels["build-version"])
                self.assertGreater(int(envs["GAME_SERVER_PROTOCOL_VERSION"]["value"]), 0)
                self.assertEqual(envs["GAMESERVERCONTROL_INTERNAL_TOKEN"]["valueFrom"]
                                 ["secretKeyRef"]["name"], "sslm-server-control")
                self.assertNotIn("value", envs["GAMESERVERCONTROL_INTERNAL_TOKEN"])

                autoscaler = scalers[f"sslm-{filename}"]
                self.assertEqual(autoscaler["spec"]["policy"]["type"], mode)
                if mode == "Counter":
                    self.assertEqual(envs["AGONES_PLAYER_COUNTER_KEY"]["value"], "players")
                    self.assertEqual(
                        fleet["spec"]["template"]["spec"]["counters"]["players"]["capacity"], capacity
                    )
                else:
                    self.assertNotIn("AGONES_PLAYER_COUNTER_KEY", envs)

    def test_allocator_rbac_is_create_only(self):
        """后端只能创建 Allocation，不能取得修改 Fleet 或读取任意 Secret 的权限。"""
        if self.yaml is None:
            self.skipTest("需要 PyYAML 校验 RBAC YAML")
        with (ROOT / "Deploy" / "Agones" / "allocator-rbac.yaml").open(
            encoding="utf-8"
        ) as source:
            documents = list(self.yaml.safe_load_all(source))
        role = next(item for item in documents if item["kind"] == "Role")
        self.assertEqual(role["rules"][0]["resources"], ["gameserverallocations"])
        self.assertEqual(role["rules"][0]["verbs"], ["create"])


if __name__ == "__main__":
    unittest.main()

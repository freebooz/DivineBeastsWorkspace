#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟真实专用服务器天气转换、两客户端及重入验证（UE5.8 Development）。

调用方：独立测试人员/CI，通过已有构建及打包产物测试，非正式游戏客户端模块。
真实网络：Dedicated Server通过仅非Shipping可用的显式命令行开发审核选项提交
小雪0.23 -> 大雪0.85 -> 晴天0 -> 再次大雪0.85；平台负责权威复制。
两个客户端在WeatherClient事件后核对Landscape MID的雪量与湿润值；B退出重进，
A继续留在服务器。生产天气默认行为、共享地图和网络协议不做修改。
限制：全部客户端使用NullRHI，因此不能证明雪花视觉品质、帧率或GPU透支。
输出：本次各进程原始日志与可复核JSON；异常时只清理由本脚本启动的精确PID，
不操作已运行的编辑器、不覆盖同名测试结果、不使用服务器作弊RPC。
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path

SERVER_STEP = re.compile(
    r"VILLAGE_SNOW_REVIEW_SERVER_STEP step=(\d+) type=(\d+) "
    r"snow=([\d.]+) revision=(\d+) accepted=(\d+)"
)
CLIENT_SAMPLE = re.compile(
    r"VILLAGE_SNOW_NET_VISUAL_SAMPLE type=(\d+) snow=([\d.]+) "
    r"wet=([\d.]+) intensity=([\d.]+) mid=([-\d.]+) matching=(\d+) count=(\d+)"
)
EXPECTED_SNOW = (0.23, 0.85, 0.0, 0.85)


def poll_processes(processes: list[subprocess.Popen], seconds: float) -> None:
    """分小段等待，捕获异常退出而不是耗尽全部测试窗口。"""
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        for proc in processes:
            code = proc.poll()
            if code is not None:
                raise RuntimeError(f"UE测试进程意外退出 pid={proc.pid} exit={code}")
        time.sleep(min(0.25, max(0.0, deadline - time.monotonic())))


def check_log(path: Path) -> str:
    if not path.is_file() or path.stat().st_size < 2048:
        raise RuntimeError(f"未取得本轮原生游戏日志：{path}")
    return path.read_text(encoding="utf-8-sig", errors="replace")


def parse_samples(path: Path) -> list[dict]:
    data = check_log(path)
    if "VILLAGE_SNOW_MID_VERIFY_SUCCESS" not in data:
        raise RuntimeError(f"没有真实Landscape材质贴图/标量绑定回读：{path}")
    samples = []
    for match in CLIENT_SAMPLE.finditer(data):
        row = {
            "type": int(match[1]),
            "snow": float(match[2]),
            "wetness": float(match[3]),
            "intensity": float(match[4]),
            "mid": float(match[5]),
            "matched": int(match[6]),
            "midCount": int(match[7]),
        }
        if (row["matched"] != 1 or row["midCount"] <= 0 or
                abs(row["snow"] - row["mid"]) > .008):
            raise RuntimeError(f"客户端已收到天气却未正确更新Landscape MID：{path} {row}")
        samples.append(row)
    if len(samples) < 3:
        raise RuntimeError(f"客户端WeatherClient/MID采样数量不足：{path}, n={len(samples)}")
    return samples


def has_band(rows: list[dict], lo: float, hi: float) -> bool:
    return any(lo <= row["snow"] <= hi for row in rows)


def main() -> None:
    parser = argparse.ArgumentParser(description="真实Village服务端天气复制与客户端MID回读测试")
    parser.add_argument("--client-stage", type=Path, required=True)
    parser.add_argument("--server-stage", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--port", type=int, default=29783)
    opts = parser.parse_args()
    if not 20000 <= opts.port <= 59000:
        raise ValueError("测试隔离UDP端口必须在20000..59000")
    client_stage = opts.client_stage.resolve()
    server_stage = opts.server_stage.resolve()
    client = client_stage / "DivineBeastsArena/Binaries/Win64/DivineBeastsArenaClient.exe"
    server = server_stage / "DivineBeastsArena/Binaries/Win64/DivineBeastsArenaServer.exe"
    if not client.is_file() or not server.is_file():
        raise FileNotFoundError(f"正式Stage Client/Server不存在：{client} {server}")
    output = opts.output_dir.resolve()
    if output.exists():
        raise FileExistsError(f"禁止覆盖同名测试运行证据：{output}")
    output.mkdir(parents=True)
    log_server = output / "VillageDedicatedServer.log"
    log_a = output / "ClientStableA.log"
    log_b1 = output / "ClientB_First.log"
    log_b2 = output / "ClientB_Rejoined.log"
    created: list[subprocess.Popen] = []
    start = time.monotonic()

    def spawn(executable: Path, args: list[str], cwd: Path) -> subprocess.Popen:
        process = subprocess.Popen(
            [str(executable), *args], cwd=str(cwd),
            stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        created.append(process)
        print("VILLAGE_SNOW_PROCESS_CREATED", executable.name, process.pid, flush=True)
        return process

    def spawn_client(log: Path) -> subprocess.Popen:
        return spawn(client, [
            f"127.0.0.1:{opts.port}", "-NullRHI", "-NoSound",
            "-nosplash", "-unattended", "-CustomConfig=VillageDevelopmentClient",
            "-textconfig", "-DBAVillageSnowTraceWeather", f"-abslog={log}"
        ], client_stage)

    try:
        srv = spawn(server, [
            "/DBAWorldPack_Village/Maps/L_Village_Start",
            f"-port={opts.port}", "-NullRHI", "-NoSound", "-nosplash",
            "-unattended", "-CustomConfig=VillageServer", "-textconfig",
            "-DBAVillageSnowReviewSchedule", f"-abslog={log_server}",
        ], server_stage)
        poll_processes([srv], 2.0)
        a = spawn_client(log_a)
        b1 = spawn_client(log_b1)
        # 等待两名客户端真正PostLogin后开始：小雪保持9秒，大雪保持26秒；
        # 冷启动约十余秒，本测试在首轮大雪期间断开B，不能假定进程启动即已进世界。
        poll_processes([srv, a, b1], 32.0)
        b1.terminate()
        try:
            b1.wait(timeout=2)
        except subprocess.TimeoutExpired:
            b1.kill()
            b1.wait(timeout=2)
        print("VILLAGE_SNOW_REVIEW_CLIENT_B_DISCONNECTED", b1.pid, flush=True)
        poll_processes([srv, a], 0.8)
        b2 = spawn_client(log_b2)
        # B重入约12秒后应该仍在大雪阶段；随后晴天12秒，再次大雪。
        poll_processes([srv, a, b2], 44.0)
    finally:
        for proc in reversed(created):
            try:
                if proc.poll() is None:
                    proc.terminate()
                    try:
                        proc.wait(timeout=1.5)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait(timeout=2)
            except OSError:
                pass
    elapsed = time.monotonic() - start
    text_server = check_log(log_server)
    matches = list(SERVER_STEP.finditer(text_server))
    if len(matches) != 4:
        raise RuntimeError(f"服务器应发布4个天气阶段，实际{len(matches)}；检查 {log_server}")
    steps = []
    for i, match in enumerate(matches):
        step, weather_type, snow, revision, accepted = (
            int(match[1]), int(match[2]), float(match[3]),
            int(match[4]), int(match[5])
        )
        if step != i or accepted != 1 or abs(snow - EXPECTED_SNOW[i]) > .015:
            raise RuntimeError(f"服务器权威天气阶段或接纳不正确：step={i} {match.group(0)}")
        if steps and revision <= steps[-1]["revision"]:
            raise RuntimeError("服务器权威天气Revision不是严格递增")
        steps.append({"step":step, "type":weather_type,"snow":snow,"revision":revision})
    joins = len(re.findall(r"Join succeeded", text_server))
    if joins < 3:
        raise RuntimeError(f"服务器观察到的Join成功数不足3，实际{joins}")
    samples_a, samples_b1, samples_b2 = (
        parse_samples(log_a), parse_samples(log_b1), parse_samples(log_b2)
    )
    if not all((
        has_band(samples_a, .17, .34),
        has_band(samples_a, .72, .91),
        has_band(samples_a, 0, .055),
    )):
        raise RuntimeError("常驻A客户端未记录小雪/大雪/晴天全部阶段")
    if not has_band(samples_b2, .70, .91) or not has_band(samples_b2, 0, .055):
        raise RuntimeError("重入B客户端未取得大雪和晴天的权威快照")
    report = {
        "schemaVersion": 1,
        "recordedAt": datetime.now(timezone.utc).isoformat(),
        "port":opts.port, "durationSeconds":round(elapsed,2),
        "serverWeatherStages":steps, "serverSuccessfulJoins":joins,
        "clientASampleCount":len(samples_a),
        "clientBFirstSampleCount":len(samples_b1),
        "clientBRejoinedSampleCount":len(samples_b2),
        "serverLog":str(log_server),
        "clientLogs":[str(log_a),str(log_b1),str(log_b2)],
        "conclusionZh":"非Shipping开发测试参数下，真实Dedicated Server天气状态复制、两端Landscape MID一致性与B重入通过",
        "excludedZh":"-NullRHI没有GPU画面，不代表Niagara视觉、性能及人工签收通过",
    }
    (output/"SnowWeatherReplicationReport.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print("VILLAGE_SNOW_WEATHER_REPLICATION_PASS", "steps=4",
          f"joins={joins}", f"A_samples={len(samples_a)}",
          f"B_rejoined_samples={len(samples_b2)}",
          f"elapsed={elapsed:.1f}s",flush=True)


if __name__ == "__main__":
    main()

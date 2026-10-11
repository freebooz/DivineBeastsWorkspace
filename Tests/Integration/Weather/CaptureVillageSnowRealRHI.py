#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""以真RHI（非NullRHI）触发Village大雪Landscape截图的非Shipping客户端回归。

前置：客户端和服务器Build+Cook+Stage分别成功，并且有单独授权的本地测试端口。
真正大雪经GamePlatformWeather服务器定时提交，两名客户端正常PostLogin：
其中A使用-RenderOffscreen保持真实GPU渲染、不聚焦用户桌面；
B使用-NullRHI仅帮助完成双客户端准入条件，无法参与GPU质量判断。
客户端项目WorldSubsystem收到大雪(.78+)且真实MID回读后两秒请求隐藏UMG的截图。
只停止本脚本创建的进程；不会触碰正在运行的Unreal Editor或生产地图。
程序只声明有截图与运行日志，不声明画面好坏、真实GPU预算或人工签收通过。
"""

from __future__ import annotations

import argparse
import json
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description="非Shipping Village真GPU大雪审查截图")
    parser.add_argument("--client-stage", type=Path, required=True)
    parser.add_argument("--server-stage", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--port", type=int, default=29785)
    parser.add_argument("--duration-seconds", type=int, default=54)
    parser.add_argument("--render-mode", choices=("offscreen", "windowed"), default="offscreen")
    opts = parser.parse_args()
    if not 20000 <= opts.port <= 59000:
        raise ValueError("只允许独立开发验证端口20000..59000")
    client_stage = opts.client_stage.resolve()
    server_stage = opts.server_stage.resolve()
    client = client_stage / "DivineBeastsArena/Binaries/Win64/DivineBeastsArenaClient.exe"
    server = server_stage / "DivineBeastsArena/Binaries/Win64/DivineBeastsArenaServer.exe"
    for exe in (client, server):
        if not exe.is_file():
            raise FileNotFoundError("未找到独立正式Stage目标：" + str(exe))
    output = opts.output_dir.resolve()
    if output.exists():
        raise FileExistsError("为保证真实审查证据不可被重写，输出目录必须全新：" + str(output))
    output.mkdir(parents=True)
    log_server = output / "VillageServer.log"
    log_gpu = output / "VillageClientGPU.log"
    log_helper = output / "VillageClientHelper.log"
    created = []

    def start(executable: Path, args: list[str], cwd: Path) -> subprocess.Popen:
        process = subprocess.Popen(
            [str(executable), *args],
            cwd=str(cwd), stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )
        created.append(process)
        print("SNOW_VISUAL_REAL_PROCESS", executable.name, process.pid, flush=True)
        return process

    started = time.monotonic()
    try:
        server_proc = start(server, [
            "/DBAWorldPack_Village/Maps/L_Village_Start", f"-port={opts.port}",
            "-NullRHI", "-NoSound", "-unattended", "-nosplash",
            "-CustomConfig=VillageServer", "-textconfig",
            "-DBAVillageSnowReviewSchedule", f"-abslog={log_server}"
        ], server_stage)
        time.sleep(2)
        if server_proc.poll() is not None:
            raise RuntimeError("正式Village服务器在客户端连接前异常退出")
        display_args = (["-RenderOffscreen"] if opts.render_mode == "offscreen"
                        else ["-windowed"])
        gpu_proc = start(client, [
            f"127.0.0.1:{opts.port}", "-NoSound", "-nosplash",
            "-unattended", "-ResX=1280", "-ResY=720",
            "-CustomConfig=VillageDevelopmentClient", "-textconfig",
            "-DBAVillageSnowTraceWeather", "-DBAVillageSnowCaptureReview",
            *display_args, f"-abslog={log_gpu}"
        ], client_stage)
        helper_proc = start(client, [
            f"127.0.0.1:{opts.port}", "-NullRHI", "-NoSound", "-unattended",
            "-nosplash", "-CustomConfig=VillageDevelopmentClient",
            "-textconfig", f"-abslog={log_helper}"
        ], client_stage)
        # 给真实GPU/Shader准备和玩家地图载入留足时间，直到大雪且2秒截图发生。
        deadline = time.monotonic() + opts.duration_seconds
        while time.monotonic() < deadline:
            for proc in (server_proc, gpu_proc, helper_proc):
                if proc.poll() is not None:
                    raise RuntimeError(f"GPU截图过程提前退出 pid={proc.pid},code={proc.returncode}")
            time.sleep(0.3)
    finally:
        for proc in reversed(created):
            if proc.poll() is None:
                try:
                    proc.terminate()
                    proc.wait(timeout=1.5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=1.5)
    if not log_gpu.is_file() or not log_server.is_file():
        raise RuntimeError("缺少真实GPU客户端或权威服务器日志")
    client_log = log_gpu.read_text(encoding="utf-8-sig", errors="replace")
    server_log = log_server.read_text(encoding="utf-8-sig", errors="replace")
    screenshot_requested = "VILLAGE_SNOW_REVIEW_SCREENSHOT_REQUESTED" in client_log
    screenshot_skipped = "VILLAGE_SNOW_REVIEW_SCREENSHOT_SKIPPED" in client_log
    report = {
        "runAt":datetime.now(timezone.utc).isoformat(),
        "renderMode":opts.render_mode,
        "durationSeconds":round(time.monotonic()-started,2),
        "serverSuccessfulJoins":server_log.count("Join succeeded"),
        "serverSnowStages":server_log.count("VILLAGE_SNOW_REVIEW_SERVER_STEP"),
        "clientMIDBound": "VILLAGE_SNOW_MID_VERIFY_SUCCESS" in client_log,
        "clientHeavySnow": "VILLAGE_SNOW_NET_VISUAL_SAMPLE type=5" in client_log,
        "screenshotRequested":screenshot_requested,
        "screenshotSkipped":screenshot_skipped,
        "clientLog":str(log_gpu), "serverLog":str(log_server),
        "resultBoundaryZh":"仅测试真实GPU视口、截图指令和是否记录像素文件；不能单独证明雪花和材质效果合格、GPU预算达标或人工视觉验收通过",
    }
    (output/"VillageSnowVisualCaptureReport.json").write_text(
        json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("VILLAGE_SNOW_VISUAL_CAPTURE_PROBE",
          f"joins={report['serverSuccessfulJoins']}",
          f"mid={report['clientMIDBound']}",
          f"heavy={report['clientHeavySnow']}",
          f"requested={screenshot_requested}",
          f"skipped={screenshot_skipped}",
          f"duration={report['durationSeconds']}",flush=True)
    if report["serverSuccessfulJoins"] < 2 or not report["clientMIDBound"]:
        raise RuntimeError("视觉测试未达到双客户端进Village与Landscape MID绑定前置条件")
    if not report["clientHeavySnow"] or not screenshot_requested:
        raise RuntimeError("真GPU大雪测试没有发出有效截图请求；检查客户端日志与窗口环境")


if __name__ == "__main__":
    main()

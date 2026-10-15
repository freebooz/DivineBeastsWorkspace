#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM Fluid Flux 3.0 角色工作室同场景性能A/B样本比较。
CSV是经过UE Insights/性能日志人工核验后标准化的原始数值输入，脚本绝不生成测量值。
输入列：Scene,Mode,Quality,Resolution,Frame,GPUms,GameThreadMs,RenderThreadMs,VRAMMiB
Mode只能为Native（原生水面）或FluidFlux（第三方局部模拟）。
"""

from __future__ import annotations

import argparse
import csv
import json
import math
from collections import defaultdict
from pathlib import Path


REQUIRED = (
    "Scene", "Mode", "Quality", "Resolution", "Frame",
    "GPUms", "GameThreadMs", "RenderThreadMs", "VRAMMiB",
)
MODES = frozenset(("Native", "FluidFlux"))
BUDGETS = {
    # 项目初始审查预算，不是官方实测，也不是显卡性能保证。
    "Low": 1.0,
    "Medium": 1.5,
    "High": 2.0,
    "Cinematic": 3.0,
}


def percentile(numbers: list[float], q: float) -> float:
    """线性插值百分位，样本数很少时仍固定行为；入参由校验器保证非空。"""
    values = sorted(numbers)
    point = q * (len(values) - 1)
    lo, hi = int(math.floor(point)), int(math.ceil(point))
    return values[lo] + (values[hi] - values[lo]) * (point - lo)


def read_samples(path: Path) -> dict[tuple[str, str, str], dict[str, list[dict]]]:
    groups: dict[tuple[str, str, str], dict[str, list[dict]]] = defaultdict(
        lambda: {"Native": [], "FluidFlux": []})
    with path.open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames or not set(REQUIRED).issubset(reader.fieldnames):
            raise ValueError("CSV缺少标准字段：" + ",".join(REQUIRED))
        for lineno, raw in enumerate(reader, start=2):
            if raw["Mode"] not in MODES or raw["Quality"] not in BUDGETS:
                raise ValueError(f"第{lineno}行Mode或Quality非法")
            if not raw["Scene"] or not raw["Resolution"]:
                raise ValueError(f"第{lineno}行场景或分辨率为空")
            try:
                record = {
                    "Frame": int(raw["Frame"]),
                    **{name: float(raw[name]) for name in
                       ("GPUms", "GameThreadMs", "RenderThreadMs", "VRAMMiB")},
                }
            except (TypeError, ValueError) as exc:
                raise ValueError(f"第{lineno}行采样不是有效数值") from exc
            if record["Frame"] < 0 or any(not math.isfinite(v) or v <= 0 for k, v in record.items()
                                           if k != "Frame"):
                raise ValueError(f"第{lineno}行存在非有限或非正的时间/显存")
            key = (raw["Scene"], raw["Quality"], raw["Resolution"])
            groups[key][raw["Mode"]].append(record)
    if not groups:
        raise ValueError("性能CSV没有任何采样")
    return groups


def compare(path: Path, warmup: int, min_samples: int,
            frame_budget_ms: float = 16.667) -> dict:
    """计算真实输入数据的差值，拒绝少样本、缺失模式及重复帧；不插入估计数。"""
    if warmup < 0 or min_samples < 30 or frame_budget_ms <= 0:
        raise ValueError("采样门禁参数非法")
    result = []
    failures = []
    for key, buckets in sorted(read_samples(path).items()):
        scene, quality, resolution = key
        processed = {}
        for mode in ("Native", "FluidFlux"):
            readings = sorted(buckets[mode], key=lambda r: r["Frame"])
            if len({r["Frame"] for r in readings}) != len(readings):
                failures.append(f"{scene}/{quality}/{resolution}/{mode}：有重复帧号")
                continue
            if len(readings) - warmup < min_samples:
                failures.append(f"{scene}/{quality}/{resolution}/{mode}：冷启动剔除后少于{min_samples}帧")
                continue
            processed[mode] = readings[warmup:]
        if len(processed) != 2:
            continue
        n, f = processed["Native"], processed["FluidFlux"]
        stats = {}
        for column in ("GPUms", "GameThreadMs", "RenderThreadMs", "VRAMMiB"):
            stats[column] = {
                "NativeP95": round(percentile([r[column] for r in n], .95), 4),
                "FluidFluxP95": round(percentile([r[column] for r in f], .95), 4),
            }
            stats[column]["AddedP95"] = round(
                stats[column]["FluidFluxP95"] - stats[column]["NativeP95"], 4)
        limit = BUDGETS[quality]
        gpu_delta = stats["GPUms"]["AddedP95"]
        gpu_total = stats["GPUms"]["FluidFluxP95"]
        passed = gpu_delta <= limit and gpu_total <= frame_budget_ms
        if not passed:
            failures.append(
                f"{scene}/{quality}/{resolution}：FluidFlux GPU P95增量{gpu_delta}ms/"
                f"预算{limit}ms，完整GPU P95={gpu_total}ms/总预算{frame_budget_ms}ms")
        result.append({
            "Scene": scene, "Quality": quality, "Resolution": resolution,
            "WarmupExcludedPerMode": warmup,
            "NativeSamples": len(n), "FluidFluxSamples": len(f),
            "GpuIncrementBudgetMs": limit,
            "GpuTotalBudgetMs": frame_budget_ms,
            "Metrics": stats, "Passed": passed,
        })
    return {
        "Passed": bool(result) and not failures,
        "SourceCSV": str(path.resolve()),
        "EvidenceNote": "仅比较用户提供的数据，未证明采集自真实UE5.8；不替代视觉、Cook和双端验收",
        "Results": result, "Failures": failures,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Fluid Flux 3.0 / 原生水体A-B性能对比")
    parser.add_argument("--input", type=Path, required=True, help="UE日志归一化后的逐帧性能CSV")
    parser.add_argument("--report", type=Path, help="可选JSON报告，需指向已存在的受控目录")
    parser.add_argument("--warmup", type=int, default=30, help="每模式剔除前30帧（非GPU测量）")
    parser.add_argument("--min-samples", type=int, default=120, help="每模式至少120帧有效测量")
    parser.add_argument("--gpu-total-budget-ms", type=float, default=16.667,
                        help="客户端总GPU帧时目标；默认60FPS对应的16.667毫秒")
    args = parser.parse_args()
    try:
        outcome = compare(args.input, args.warmup, args.min_samples,
                          args.gpu_total_budget_ms)
        text = json.dumps(outcome, ensure_ascii=False, indent=2)
        if args.report:
            if not args.report.parent.is_dir():
                raise ValueError("报告目录不存在；拒绝在项目外自动创建未知目录")
            args.report.write_text(text, encoding="utf-8")
        print(text)
        return 0 if outcome["Passed"] else 1
    except (ValueError, OSError) as exc:
        print(json.dumps({"Passed": False, "Error": str(exc)}, ensure_ascii=False))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

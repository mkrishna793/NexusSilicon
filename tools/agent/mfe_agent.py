#!/usr/bin/env python3
"""Evidence-first, read-only assistant for Metric Field Engine projects.

It works offline for deterministic diagnostics, or uses any OpenAI-compatible
chat endpoint when MFE_LLM_BASE_URL and MFE_LLM_MODEL are configured.  The
assistant never changes RTL, constraints, layouts, or ECO files.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path


def project_results(project: Path) -> Path:
    outputs = "results"
    for raw in project.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if line.startswith("outputs:"):
            outputs = line.split(":", 1)[1].strip().strip('"\'')
            break
    return project.parent / outputs


def read_json(path: Path):
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {"unreadable": str(path)}


def collect_evidence(project: Path) -> dict:
    results = project_results(project)
    timing = results / "timing"
    physical = results / "physical"
    return {
        "project": str(project),
        "results": str(results),
        "timing_evidence": read_json(timing / "timing-evidence.json"),
        "eco_plan": read_json(timing / "eco-plan.json"),
        "timing_stage": read_json(timing / "stage-result.json"),
        "physical_stage": read_json(physical / "stage-result.json"),
        "physical_evidence": read_json(physical / "evidence-graph.json"),
    }


def offline_answer(question: str, evidence: dict) -> str:
    findings = []
    timing_graph = evidence.get("timing_evidence") or {}
    for entity in timing_graph.get("entities", []):
        if entity.get("kind") == "finding":
            findings.append(entity.get("display_name", "unknown finding"))
    metrics = ((evidence.get("physical_stage") or {}).get("metrics") or {})
    lines = [
        "MFE offline evidence answer (no model call; no design was changed).",
        f"Question: {question}",
        f"Project: {evidence['project']}",
    ]
    if findings:
        lines.append("Timing findings:")
        lines.extend(f"- {finding}" for finding in findings[:8])
    else:
        lines.append("No parsed timing findings are available. Run the flow and timing/ECO stages first.")
    if metrics:
        selected = [f"{key}={metrics[key]}" for key in ("routed_nets_count", "steiner_routed_nets", "total_route_length_um") if key in metrics]
        if selected:
            lines.append("Physical evidence: " + ", ".join(selected))
    lines.append("Recommended next action: review the ECO plan, then run an explicitly approved ECO/re-place/re-route/STA loop.")
    return "\n".join(lines)


def remote_answer(question: str, evidence: dict) -> str:
    base_url = os.environ.get("MFE_LLM_BASE_URL", "").rstrip("/")
    model = os.environ.get("MFE_LLM_MODEL", "")
    if not base_url or not model:
        raise RuntimeError("Set MFE_LLM_BASE_URL and MFE_LLM_MODEL, or use --offline.")
    api_key = os.environ.get("MFE_LLM_API_KEY", "")
    prompt = (
        "You are the read-only MFE evidence assistant. Answer only from the JSON evidence. "
        "State uncertainty. Do not claim signoff, tapeout readiness, DRC/LVS closure, or apply changes. "
        "Recommend a reviewed next experiment, citing finding/path IDs when available.\n\n"
        f"User question: {question}\n\nEvidence:\n{json.dumps(evidence, ensure_ascii=False)[:120000]}"
    )
    payload = json.dumps({"model": model, "messages": [{"role": "user", "content": prompt}], "temperature": 0.1}).encode()
    request = urllib.request.Request(
        base_url + "/chat/completions", data=payload,
        headers={"Content-Type": "application/json", **({"Authorization": f"Bearer {api_key}"} if api_key else {})},
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=90) as response:
            output = json.loads(response.read().decode("utf-8"))
    except urllib.error.URLError as error:
        raise RuntimeError(f"LLM endpoint failed: {error}") from error
    return output["choices"][0]["message"]["content"]


def main() -> int:
    parser = argparse.ArgumentParser(description="Read-only MFE evidence assistant")
    parser.add_argument("ask", help="question to investigate")
    parser.add_argument("--project", required=True, type=Path, help="path to project.yaml")
    parser.add_argument("--offline", action="store_true", help="do not contact an LLM endpoint")
    args = parser.parse_args()
    if not args.project.is_file():
        parser.error(f"project manifest not found: {args.project}")
    evidence = collect_evidence(args.project.resolve())
    try:
        print(offline_answer(args.ask, evidence) if args.offline else remote_answer(args.ask, evidence))
    except RuntimeError as error:
        print(f"MFE agent error: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

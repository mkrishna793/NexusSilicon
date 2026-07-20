import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
project = root / "projects" / "gcd_demo" / "project.yaml"
result = subprocess.run(
    [sys.executable, str(root / "tools" / "agent" / "mfe_agent.py"), "what failed?", "--project", str(project), "--offline"],
    check=False, capture_output=True, text=True,
)
assert result.returncode == 0, result.stderr
assert "MFE offline evidence answer" in result.stdout

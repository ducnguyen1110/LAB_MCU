"""Run the actual scheduler and main.c demo callbacks with host HAL stubs."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "Firmware/Core/Src/main.c").read_text(encoding="utf-8")

def region(name):
    return source.split(f"/* USER CODE BEGIN {name} */", 1)[1].split(
        f"/* USER CODE END {name} */", 1)[0]

compiler = shutil.which("gcc")
if compiler is None:
    raise SystemExit("A native GCC compiler is required for these host tests.")

with tempfile.TemporaryDirectory(prefix="lab4-tests-") as directory:
    output = Path(directory)
    (output / "app_sections.h").write_text(
        region("PD") + region("PV") + region("0")
        + "\nstatic void demo_setup(void) {\n" + region("2") + "\n}\n",
        encoding="utf-8",
    )
    binary = output / "test_lab4.exe"
    subprocess.run([
        compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-DSCH_HOST_TEST",
        "-I", str(output), "-I", str(root / "Firmware/Core/Inc"),
        str(root / "tests/test_lab4.c"), "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)

"""Build using an already-installed MinGW g++; installs nothing."""
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
(root / 'build').mkdir(exist_ok=True)
sources = ['src/ring_buffer.cpp', 'src/protocol_parser.cpp', 'src/tcp_client.cpp', 'src/robot_sdk.cpp']
for name, source in [('protocol_tests','tests/protocol_tests.cpp'), ('integration_client','tests/integration_client.cpp'), ('robot_demo','examples/robot_demo.cpp')]:
    cmd = ['g++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-D_WIN32_WINNT=0x0600', '-DNOMINMAX', '-Iinclude', *sources, source, '-o', f'build/{name}.exe', '-lws2_32']
    print(' '.join(cmd), flush=True)
    subprocess.run(cmd, cwd=root, check=True)

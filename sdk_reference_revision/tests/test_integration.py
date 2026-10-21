"""Runs the compiled SDK against an independent Python wire implementation."""
import json
from pathlib import Path
import socket
import subprocess
import sys
import threading
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from mock_robot import MockRobot

def run(executable):
    results = []
    for mode in ['normal', 'faults', 'reconnect', 'silent', 'callback_error', 'absent', 'delayed']:
        server = None
        starter = None
        holder = []
        if mode in ('absent', 'delayed'):
            reservation = socket.socket()
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
            if mode == 'delayed':
                reservation.close()
                def launch():
                    time.sleep(0.5)
                    holder.append(MockRobot('normal', port).start())
                starter = threading.Thread(target=launch)
                starter.start()
        else:
            server = MockRobot('normal' if mode == 'callback_error' else mode).start()
            port = server.port
        try:
            started = time.monotonic()
            result = subprocess.run([str(executable), str(port), mode], capture_output=True, text=True, timeout=10)
            elapsed = time.monotonic() - started
            print(result.stdout.strip(), flush=True)
            if result.returncode:
                raise AssertionError(f'{mode}: {result.stderr} {result.stdout}')
            results.append({'mode': mode, 'passed': True, 'elapsed_seconds': round(elapsed, 3), 'output': result.stdout.strip()})
        finally:
            if starter:
                starter.join(timeout=2)
            if holder:
                holder[0].stop()
            if server:
                server.stop()
            if mode == 'absent':
                reservation.close()
    return results

if __name__ == '__main__':
    results = run(Path(sys.argv[1]).resolve())
    if len(sys.argv) > 2:
        Path(sys.argv[2]).write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'PASS integration scenarios={len(results)}')

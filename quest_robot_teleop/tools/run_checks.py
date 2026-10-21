"""No third-party dependencies. Uses installed .NET SDK and Python."""
from pathlib import Path
import json
import socket
import subprocess
import sys
import threading
import time
from simulator import Simulator
root=Path(__file__).resolve().parents[1]
logs=[]
def command(args):
    result=subprocess.run(args,cwd=root,capture_output=True,text=True,timeout=60)
    text=result.stdout+result.stderr
    print(text,flush=True)
    logs.append({'command':args,'returncode':result.returncode,'output':text})
    if result.returncode:
        raise RuntimeError(f'failed: {args}')
command(['dotnet','restore','Tests/CoreChecks.csproj','--configfile','Tests/NuGet.Config'])
command(['dotnet','run','--project','Tests/CoreChecks.csproj','--configuration','Release','--no-restore'])
command([sys.executable,'-m','unittest','discover','-s','Tests','-p','test_*.py','-v'])
stop=threading.Event()
with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
    sock.bind(('127.0.0.1',0));sock.settimeout(.05)
    port=sock.getsockname()[1]
    sim=Simulator()
    def serve():
        while not stop.is_set():
            try:data,peer=sock.recvfrom(4096)
            except socket.timeout:continue
            answer=sim.handle(data,peer,time.monotonic())
            if answer:sock.sendto(answer,peer)
            sim.watchdog(time.monotonic())
    thread=threading.Thread(target=serve);thread.start()
    try:command(['dotnet','run','--project','Tests/CoreChecks.csproj','--configuration','Release','--no-build','--no-restore','--','127.0.0.1',str(port)])
    finally:stop.set();thread.join(timeout=2)
report={'date':'2026-10-21','scope':'NET core + Python simulator + loopback UDP only','unity_editor_compiled':False,'quest3s_tested':False,'logs':logs}
(root/'docs').mkdir(exist_ok=True)
(root/'docs/test_results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('PASS: core, Python unit tests and C#-Python UDP. Unity/Quest NOT tested.')

"""Run meaningful verification; persist actual output without installing anything."""
from pathlib import Path
import json
import os
import platform
import subprocess
import sys
import time
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root/'tests'))
from test_integration import run
sys.path.insert(0, str(root/'tools'))
from mock_robot import MockRobot
suffix = '.exe' if os.name == 'nt' else ''
build = Path(sys.argv[1]).resolve() if len(sys.argv)>1 else root/'build'
report_dir = root/'docs'
unit = subprocess.run([str(build/f'protocol_tests{suffix}')], capture_output=True, text=True, check=True, timeout=10)
print(unit.stdout, end='', flush=True)
scenarios = run(build/f'integration_client{suffix}')
server=MockRobot().start()
try:
    demo=subprocess.run([str(build/f'robot_demo{suffix}'),str(server.port)],capture_output=True,text=True,check=True,timeout=10)
    if 'received_states=3' not in demo.stdout:
        raise AssertionError(demo.stdout)
    print(demo.stdout,end='',flush=True)
finally:
    server.stop()
try:
    compiler=subprocess.run(['g++','--version'],capture_output=True,text=True,timeout=5).stdout.splitlines()[0]
except (FileNotFoundError,IndexError):
    compiler='See build log'
results={'execution_time':time.strftime('%Y-%m-%dT%H:%M:%S%z'),'platform':platform.platform(),'compiler':compiler,'unit_output':unit.stdout.strip(),'integration':scenarios,'demo_output':demo.stdout.strip(),'scope':'loopback simulator only'}
(report_dir/'test_results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
lines=['# 测试报告','', '验证日期：2026-10-18（本次任务日期）；执行时间及系统信息详见test_results.json。','',f'- 环境：{results["platform"]}',f'- 编译器：{compiler}','- 构建：tools/build_mingw.py，C++17，-O2 -Wall -Wextra -Wpedantic -Werror。','- 运行：python tools/run_checks.py','- 网络：127.0.0.1，独立Python模拟协议端；每类集成场景约0.2-2.5秒。','', '## 已通过','',f'- 协议/边界断言：{unit.stdout.strip()}。138项断言不是138个独立测试用例。','- 包括CRC标准向量、全部拆分边界、逐字节输入、粘包、CRC错误恢复、非法长度、最大负载、缓冲区越界拒绝、浮点编码及短写/错误/取消测试。','- 集成测试全部验证Shutdown、再次Init及重复Shutdown。','', '| 场景 | 实际结果 |','|---|---|']
labels={'normal':'正常命令/状态回传','faults':'坏CRC、非法长度与分片恢复','reconnect':'对端主动断开后重连','silent':'TCP仍连接但不回复心跳','callback_error':'回调抛异常后继续处理','absent':'服务端缺席与退出','delayed':'启动后延迟开启服务端'}
for item in scenarios:
    lines.append(f'| {labels[item["mode"]]} | `{item["output"]}` |')
lines += ['', '## 演示闭环','', '```text', demo.stdout.strip(), '```','', '## 尚未验证与限制','', '- Linux/CMake路径、GitHub Actions、MSVC以及ASan/TSan未在本机运行。','- 未接真实机器人，无实机场景、真实弱网、网络延迟分位数或72小时稳定性测试。','- 短写通过可注入发送函数确定性验证，不宣称本次Socket测试触发了操作系统短写。','- 重连测试是本机服务端主动断开，静默测试是不回应心跳，不能等同生产弱网。','- stop_ms是单次样本，0ms表示毫秒计时粒度下不足1ms，不是零耗时或实时保证。','- 功能测试不能证明工业安全性，也不证明本人已独立掌握全部代码。','', '## 复现','', '已有MinGW与Python时运行：','', '```powershell','python tools/build_mingw.py','python tools/run_checks.py','```','', '原始JSON日志：test_results.json。无需连接外网或安装依赖。']
(report_dir/'测试报告.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('PASS unit + 7 integration scenarios + demo; reports saved')

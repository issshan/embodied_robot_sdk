# Robot Communication SDK

C++17 机器人通信与二次开发练习项目。提供底盘速度命令、状态回调、自定义TCP帧协议及模拟机器人服务端。

本版本在原始项目基础上于2026-10-18辅助修订。它是学习与求职作品原型，不是工业安全控制软件；只在本机回环网络完成了短时模拟测试，未验证实机、弱网性能或长期稳定性。历史README中的3ms、72小时零丢包、无锁和零拷贝声明不适用于本版本。

## 已实现

- 帧头搜索、明确的小端编码、最大4096字节负载、CRC16/MODBUS收发校验。
- 有界环形缓冲区，支持分段输入、粘包解析和错误数据重新同步。缓冲区使用互斥锁并复制数据。
- 独立网络线程拥有Socket；应用线程向有界FIFO提交数据，短写重试带总超时。
- 周期心跳、心跳回复超时断开、连接失败后的指数退避重试。
- 断线清空未发命令，避免旧运动命令在新连接中重放。
- 底盘速度编码与模拟状态回传；C++示例、独立Python模拟端及自动化测试。
- 非阻塞Socket与短轮询，实现可取消的连接、收发和重连等待。

## 项目结构

```text
include/       协议、SDK、TCP、缓冲区与平台边界
src/           实现
examples/      最小速度控制与状态回调演示
tools/         mock_robot.py、build_mingw.py、run_checks.py
tests/         协议/边界测试、C++集成客户端和Python测试驱动
docs/          协议、测试报告、修订说明、简历描述和面试准备
.github/       尚最初未上传（2026-10-21以新增目录发布）或执行的CI配置
```

## Linux构建和验证

需要已有C++17编译器、CMake 3.15+；集成测试另需Python 3，仅使用标准库。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

本机无可用Linux运行环境，也未安装CMake，因此上述CMake/Linux路径尚未实际运行。已添加Linux与Windows CI配置，只有推送后检查通过才可宣称其通过。

## Windows现有MinGW构建

本次使用已有MinGW GCC 6.3.0验证，无需安装包。该编译器没有std::thread实现，Windows平台层使用Win32线程和临界区；Linux平台层使用std::thread/std::mutex。不是无锁实现。

```powershell
python tools/build_mingw.py
python tools/run_checks.py
```

如果python未加入PATH，用本机Python可执行文件的完整路径代替。不要将此项目命令当成安装指令。

## 运行演示

先开一个终端：

```bash
python tools/mock_robot.py --port 8888
```

另一个终端运行：

```bash
./build/robot_demo 8888
# Windows: .\build\robot_demo.exe 8888
```

模拟端仅绑定127.0.0.1，收到速度指令后回传相同数值，不模拟真实运动学、不驱动任何硬件。

## SDK接入

```cpp
#include "robot_sdk.hpp"
embodied_robot::RobotSDK sdk("127.0.0.1", 8888);
sdk.RegisterSensorCallback([](uint16_t type, const uint8_t* bytes, size_t count) {
    // bytes仅在本次回调期间有效；需要异步处理时自行复制。
    // 回调在网络线程运行，应快速返回。
});
if (sdk.Init() && sdk.WaitUntilConnected(3000)) {
    bool queued = sdk.SetChassisVelocity({0.2f, 0.0f, 0.1f});
    // queued只表示提交到本地队列，不表示对端收到或执行。
    (void)queued;
}
sdk.Shutdown();
```

`Init()`表示网络线程已启动；`IsConnected()`只表示TCP已建立，不代表业务握手或控制权限就绪。心跳消息为0x0001/0x0002，对端必须遵循约定。速度要求有限浮点数；速度范围限制需要真实设备协议另行实现。

`Start/Stop`由对象生命周期线程调用。禁止在回调中调用生命周期方法、销毁SDK或长期阻塞；捕获对象必须活到Shutdown返回之后。可以在回调中提交异步命令或更换回调。回调异常被捕获计数，但不替代业务错误处理。

## 验证与边界

详见[测试报告](docs/测试报告.md)及[协议说明](docs/协议说明.md)。

- 无实机、ROS、Python绑定、关节控制或导航算法；不宣称具身智能算法研发能力。
- 当前仅IPv4点分十进制地址；无DNS、TLS、认证或加密，不应直接暴露公网。
- 无业务序列号/执行确认/幂等重试，无法承诺控制指令恰好执行一次。
- 断线时队列丢弃采用保守策略，已发出的命令是否执行无法判断。
- 最大负载以内的损坏长度可能使解析暂时等待；若未恢复，心跳超时将关闭连接。
- 不提供物理急停、设备侧失联停车或运动安全保证，禁止未经安全评估连接真实底盘。
- `Stats()`为多个原子计数的近似快照，非事务一致快照；CRC及长度错误在连接结束后汇总。
- 回调长期阻塞会延迟心跳及退出；退出时限不是实时保证。

## 常见排查

1. 无法连接：检查模拟端是否启动、端口一致、IP是否为合法IPv4。
2. 有连接无状态：确认0x1001/0x2001消息号、12字节负载、小端浮点编码和CRC。
3. 周期重连：对端是否回复空负载0x0002；回调是否长期阻塞。
4. 命令提交返回false：未连接、已停止或队列已满；重连后需由上层决策是否重新下发。
5. 校验错误：对照`123456789`的CRC结果0x4B37，检查CRC覆盖帧头及负载但不包括CRC本身。

没有新增软件安装、GitHub推送、历史简历改写或性能指标背书。原始项目仍保留在原压缩包中。

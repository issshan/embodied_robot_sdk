# Quest Robot Teleop

面向Meta Quest 3S的机器人遥操作原型源码：Unity/C#/OpenXR输入适配，独立C#坐标与控制核心，以及Python UDP模拟对端。

**状态：2026-10-21新建，AI辅助开发。独立.NET核心、Python模拟端及本机UDP通信已测试；Unity编辑器/Android APK尚未编译，Quest 3S尚未实机验证。不能据此宣称多年VR开发经验。**

## 当前实现

- 校准控制器中立位姿，计算相对位姿与位置缩放；Unity右/上/前坐标映射到机器人前/左/上坐标。
- 四元数归一化、相对旋转和反射坐标系转换，核心以独立.NET程序测试。
- 有界工作空间、目标速度限制、扳机使能、追踪丢失/心跳超时退出Active及急停锁存；恢复后要求松开再使能。
- Unity侧头显/右手柄输入适配源码；桌面模拟输入、三自由度解析机械臂可视化、UDP命令与状态反馈、JSONL录制和仅可视化回放。
- Python模拟端对seq乱序、异常数据、重复包、会话占用及失联进行处理；没有真实设备控制驱动。

这不是6自由度机械臂控制器、物理仿真器、全身运动重定向系统或工业安全系统。机械臂为yaw+两连杆的位置演示；朝向映射被计算/记录，但不用于该3DoF机械臂末端姿态控制。

## 源码结构

```text
Assets/Teleop/Core/       无Unity依赖的几何、标定、状态门控、消息模型
Assets/Teleop/Runtime/    Quest/桌面输入、UDP、录制回放、机械臂可视化
Assets/Teleop/Editor/     桌面/Quest场景生成菜单与Android配置辅助
Packages/                固定版本的包清单
Tests/                   .NET核心验证、Python模拟端测试
 tools/                  模拟端与一键验证脚本
 docs/                   测试结果、安装后操作、协议及硬件验收清单
```

## 现在就能运行的验证

需要已有.NET 9 SDK及Python 3；测试不下载第三方包。

```bash
python tools/run_checks.py
```

该命令执行1129项核心断言、8个Python单元测试，以及C#客户端到Python模拟端的真实UDP握手/受限运动/停止闭环。结果在`docs/test_results.json`。1129是断言次数，不是1129个独立测试用例。不能把测试结果扩展为Unity/APK/头显通过。

## Unity使用步骤（尚待实际编译）

1. 使用Unity Hub打开本目录，目标Editor 6000.0.65f1；manifest固定OpenXR 1.16.1、Input System 1.14.2、XR Management 4.5.0。包解析和版本组合尚未实际验证。
2. 等待包导入；在Player Settings将Active Input Handling设为Input System Package并重启编辑器（如提示）。
3. `Teleop > Create Desktop Simulator Scene`生成场景。在电脑上运行`python tools/simulator.py`，然后Play。按C标定，松开再按住Space使能，方向键/PgUp/PgDn移动输入；E急停、R解除、T模拟追踪丢失、F录制、P回放。
4. 场景由Editor菜单生成，无外部模型和贴图依赖；仓库未附已验证的.unity场景或APK。
5. Quest配置和实机步骤见[设备验收](docs/QUEST3S_CHECKLIST.md)。Android切换、OpenXR加载器、控制器Profile与Project Validation需在编辑器中实际检查。

## 输入约定

右手柄Grip按下沿：标定（需追踪有效且扳机未按住）。右Trigger按住：持续使能。右A：软件急停锁存。右B：解除急停（需扳机释放）。左X：开/关本地录制。左Y：仅可视化回放。

这些输入为源码设计，Quest按钮映射尚未实测。设备失去焦点或应用暂停时锁存停止；回放始终不使能网络控制。恢复控制前需重新标定、解除急停、松开再按扳机。

## 网络和数据

默认仅访问`127.0.0.1:8765`模拟端。Quest运行时127.0.0.1指头显自身，必须在可信局域网设置电脑私有IP并将模拟端明确绑定到该接口。不要关闭防火墙；只在需要时由用户允许指定程序和私有网络。

协议为UTF-8 JSON/UDP，version/session/seq/target/enabled/state，端点返回ack和模拟状态。接收端本地单调时间检测超时；不依赖跨设备时钟同步，不能用该结果测单向延迟。会话字段防误混流，不提供认证。

JSONL保存在应用私有存储中，只有主动按录制键才开始；包含控制器位姿与目标状态，不上传GitHub或云。回放只可视化最近一次录制，限制读取18000帧。

## 限制

- 没有真实机器人、ROS、MuJoCo、URDF导入、PICO适配、WebRTC视频/音频、力反馈、全身重定向、眼动/裸手追踪。
- Unity端尚未编译，可能存在需要按实际包API调整的配置代码；用户选择本轮不安装Unity。
- 不含TLS、设备鉴权、硬件急停及机器人侧安全控制。勿直接接入真实设备，不对公网开放模拟器。
- 不提供延迟、帧率、内存/功耗优化成绩；界面fps只是运行时观察值。
- 本项目不能证明2023-2026年间的VR经历，也不能证明本人独立掌握AI辅助新增代码。

## 参考实现与官方文档

只作架构参考，未拷贝其实现；本项目没有集成以下项目，不宣称兼容。

- [Unity Robotics Hub](https://github.com/Unity-Technologies/Unity-Robotics-Hub)
- [SpesRobotics teleop](https://github.com/SpesRobotics/teleop)
- [Unity OpenXR 1.16](https://docs.unity3d.com/Packages/com.unity.xr.openxr@1.16/manual/index.html)
- [Oculus Touch Profile](https://docs.unity3d.com/Packages/com.unity.xr.openxr@1.16/manual/features/oculustouchcontrollerprofile.html)

项目源码可用于求职展示，简历应写真实起始时间、具体贡献与验证范围。

# 架构与范围

输入：Quest右手柄（Unity XR InputDevices）或桌面键盘 → 相对中立姿态标定 → Unity坐标到机器人坐标 → 工作空间/速度门控 → UDP模拟命令 → Python受限目标状态与ack → Unity连接状态。

本地可视化使用3DoF解析IK绘制两段连杆；它显示本地受限目标，不是Python物理动力学结果。没有碰撞求解、MuJoCo或真实机器人驱动。

## 坐标约定

Unity位置(x,y,z)映射到机器人(z,-x,y)。旋转轴向量按(-z,x,-y)映射，w保持不变并归一化。标定采用控制器中立位姿作为相对参考；不是相机-IMU-雷达联合标定或外参优化算法。

## 状态门控

Uncalibrated、Idle、Active、TrackingLost、Disconnected、EmergencyStop。失联/丢追踪会使下一次启用需要释放扳机；软件急停锁存需释放扳机再显式Reset。应用暂停触发Trip。基础输入、dt非法时fail closed。模拟端额外限制工作空间、速度和接收序列号。

## 记录与回放

JSONL行包括单调时间、输入控制器位姿、受限目标、朝向和状态。只在用户主动开启后写本地文件。回放不发送有效控制命令，不提供物理重演保证。没有多模态时钟同步与训练数据格式转换功能。

## 验证边界

.NET测试直接编译Assets/Teleop/Core同一份C#源码；不是Unity假替身。测试验证数学与门控，不验证UnityEngine API、OpenXR设备、安卓权限、渲染或JsonUtility。真实UDP闭环采用System.Text.Json（仅字段）模拟Unity字段协议，Unity实际序列化仍待编辑器验证。

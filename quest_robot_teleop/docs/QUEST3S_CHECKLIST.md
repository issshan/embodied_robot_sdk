# Quest 3S 后续验收清单

当前全为待验收，用户本轮选择不安装Unity。无APK交付。

1. 本人通过官方Unity Hub安装指定编辑器及Android Build Support、SDK/NDK/OpenJDK，完成个人账号和许可步骤。
2. 从Unity Hub打开项目，确认包解析成功并记录实际Editor与包版本；修复/记录编译错误后才可标记源码在Unity可构建。
3. 先生成Desktop场景、启用Input System并运行Python模拟端；验证标定、使能、停止、录制、回放。
4. Android平台启用OpenXR Loader，添加Oculus Touch Controller Profile与Meta Quest Support；运行Project Validation并逐项处理。Editor配置菜单只作辅助，不能替代Validation。
5. 配置ARM64、IL2CPP和Vulkan；按当前Meta要求核对SDK/Android设置。若配置菜单API与实际包不符，手动配置并修订源码后重测。
6. 生成Quest场景，关闭desktopSimulation。将serverIp改为电脑实际私有IP，运行`python tools/simulator.py --host <该IP>`。局域网没有鉴权，不使用公共WiFi或公网地址。
7. 用户本人在Meta设备中完成开发者模式、USB调试授权和安装。不要分享账号、口令或身份认证材料。
8. 验证头显与右手柄6DoF追踪有效，Grip中立标定不会跳变；逐轴检查前/左/上方向与旋转符号。
9. 检查右扳机使能、右A软件停止、右B解除，左X录制、左Y回放；追踪丢失、应用暂停、停止模拟端后不再保持Active。
10. 网络恢复后仍握住扳机不能自动恢复，需松开再使能。回放期间永不发送enabled=true。
11. 记录设备型号、系统版本、视频或截图、实际测试步骤和失败项。只有逐项通过后更新简历“Quest真机联调”表述。

本项目的停止与限幅仅用于模拟，不代表硬件安全认证。不要把下一步验收清单写成已完成成果。

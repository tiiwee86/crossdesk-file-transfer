# 常见问题（FAQ）

欢迎来到 **CrossDesk 常见问题** 页面！  
这里整理了用户和开发者最常见的一些疑问。如果你没有找到答案，欢迎在 [Issues](https://github.com/kunkundi/crossdesk/issues) 中反馈。

---

### Q1. 对等连接失败
**A:**  
打开设置，勾选 **启用中继服务** 选项，尝试重新发起连接。

<img width="396" height="306" alt="Image" src="https://github.com/user-attachments/assets/fd8db148-c782-4f4d-b874-8f1b2a7ec7d6" />

由于公共中继服务器带宽较小，连接的清晰度流畅度可能会下降，建议自建服务器。 [Issue #8](https://github.com/kunkundi/crossdesk/issues/8)

### Q2. Windows 无 CUDA 环境下编译
**A:**  
运行下面的命令安装 CUDA 编译环境。
```
xmake require -vy "cuda 12.6.3"
```
安装完成后执行
```
xmake require --info "cuda 12.6.3"
```
输出如下

<img width="860" height="226" alt="Image" src="https://github.com/user-attachments/assets/999ac365-581a-4b9a-806e-05eb3e4cf44d" />

根据上述输出获取到 CUDA 的安装目录，即 installdir 指向的位置。将 CUDA_PATH 加入系统环境变量，或在终端中输入 set CUDA_PATH=path_to_cuda_installdir，重新执行 xmake b -vy crossdesk 即可。
[Issue #6](https://github.com/kunkundi/crossdesk/issues/6)

---
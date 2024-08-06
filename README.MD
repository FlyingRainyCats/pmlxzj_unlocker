# 解除《屏幕录像专家》编辑锁定

用于解除使用屏幕录像专家对录像进行 “已进行编辑加密” 文件的解密。

解锁前：

![锁定截图](assets/locked.webp)

解锁后：

![解锁后截图](assets/unlocked.webp)

by 爱飞的猫@52pojie.cn - 仅供学习交流用途

## 特性说明

支持：

- 音频提取 (仅限 wav)
- 解除“编辑加密”锁定

测试于下述《屏幕录像专家》版本生成的 EXE 播放器文件：

- V2014 Build 0318
- V2023 Build 0828

用到了 `zlib` 进行数据解压缩操作。

## 两个版本

默认分发的二进制文件（通过 GitHub Actions 构建）有两个版本：

- `pmlxzj_unlocker-i686.exe`
  - msys2-mingw32 构建，支持 32 位系统。
  - 文件名包含非 ASCII 字符时可能有兼容性问题。
- `pmlxzj_unlocker.exe`
  - msys2-ucrt64 构建，对新系统支援较好，不支持 32 位系统。
  - 支持包含非 ASCII 字符的文件名。

## 使用方法

首先使用 cmake + mingw 工具链编译。VS 或许可以，未测试。

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build
```

然后运行 `unlock` 指令解锁：

```shell
# Win
.\build\pmlxzj_unlocker.exe unlock "录像1.exe" "录像1_解锁.exe"

# Other
./build/pmlxzj_unlocker.exe unlock "录像1.exe" "录像1_解锁.exe"
```

其它支持的指令有 `audio-dump` 提取音频，和 `info` 查看信息。

### 意义不明

有很多地方还没搞懂，也有很多参数能控制读取文件的偏移。目前只是对着部分已知的文件进行测试。

## 碎碎念

OBS 永远的神，屏幕录像专家可以说是时代的眼泪了…

XP 时代应该是它的巅峰（默认 5fps，录教程够了，低资源设备友好）。
不过感觉从 Win7 开始就有点力不从心 + 摆烂了。

本质上只是为了翻录到 MP4 格式方便传输。利用 OBS + 自动翻录脚本也能做到类似的效果。

一开始也想过所谓的“高度无损压缩”会是什么黑科技，结果却发现就是简单的 gzip 压缩有点失望。

此外逆向过程中发现了其内嵌了两个 txt 文件（换行分割的数组），怀疑是用来重放鼠标点击事件的信息。
在代码中的 `idx1` 和 `idx2` 为解析后的对应数据。

## 致谢

- Hmily 老师帮忙定位到了关键解密代码和大致解密流程。

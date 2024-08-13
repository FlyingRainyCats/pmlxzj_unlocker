# 解除《屏幕录像专家》编辑锁定

[![GitHub License](https://img.shields.io/github/license/FlyingRainyCats/pmlxzj_unlocker)](LICENSE)
[![GitHub Release](https://img.shields.io/github/v/release/FlyingRainyCats/pmlxzj_unlocker)](https://github.com/FlyingRainyCats/pmlxzj_unlocker/releases/latest)
[![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/FlyingRainyCats/pmlxzj_unlocker/build-msys2.yml)](https://github.com/FlyingRainyCats/pmlxzj_unlocker/actions/workflows/build-msys2.yml)

用于解除使用屏幕录像专家对录像进行 “已进行编辑加密” 文件的解密。

|             解锁前             |              解锁后               |
|:---------------------------:|:------------------------------:|
| ![锁定截图](assets/locked.webp) | ![解锁后截图](assets/unlocked.webp) | 

by 爱飞的猫@52pojie.cn - 仅供学习交流用途

## 特性说明

支持：

- 音频提取 (仅限 wav)
- 禁用音频[^disable_audio]
- 解除“编辑加密”锁定
- 解除“播放加密”锁定 (需要提供正确密码 [^password_note] [^non_ascii_password_note])

[^password_note]: 密码的第一位字符不参与解密。
[^non_ascii_password_note]: 包含非 ASCII 字符的密码需要储存到文件，并透过 `-P` 指定。
[^disable_audio]: 禁用音频后，《屏幕录像专家》可以将视频转码到无音频的 MP4 文件。

测试于下述《屏幕录像专家》版本生成的 EXE 播放器文件：

- V7.5 Build 200701224
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

## 编译

使用 cmake + mingw 工具链编译。VS 或许可以，未测试。

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build
```

## 使用方法

### 解锁视频

使用 `unlock` 指令解锁：

```shell
./pmlxzj_unlocker.exe unlock "录像1.exe" "录像1_解锁.exe"
./pmlxzj_unlocker.exe unlock -p password "录像1.exe" "录像1_解锁.exe"
./pmlxzj_unlocker.exe unlock -P /path/to/password.txt "录像1.exe" "录像1_解锁.exe"
```

※ 若密码包含中文或其它非 ASCII 字符，先存储至文件 (GBK 编码)，然后透过 `-P` 参数指定。

此外可以指定 `-r` 在密码错误时继续、`-v` 输出更多信息。

### 提取音频

目前只支持 `WAV` (压缩) 和 `MP3` 格式音频提取。

```shell
./pmlxzj_unlocker.exe audio-dump "录像1.exe" "录像1.wav"
```

### 禁用音频

如果 EXE 播放器启用了声音压缩，那么《屏幕录像专家》将拒绝转换视频到 MP4 等格式。

为了绕过这一限制，可以使用工具将 EXE 播放器中的音频禁用，然后进行转换。

※ 如果处理《屏幕录像专家》7.0 或之前的版本生成的 EXE 播放器，需要使用《屏幕录像专家》升级播放器然后再处理。

```shell
./pmlxzj_unlocker.exe audio-disable "录像1.exe" "录像1_无音频.exe"
```

### 查看信息

查看部分读取的信息。

```shell
./pmlxzj_unlocker.exe info "录像1.exe"
./pmlxzj_unlocker.exe info -v "录像1.exe"
```

此外可以指定 `-v` 输出更多信息。

### 意义不明

有很多地方还没搞懂，也有很多参数能控制读取文件的偏移。

目前只是对着部分已知的文件进行测试。

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

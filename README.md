<p align="center">
	<img width="72%" alt="NCM OSP" src="docs/img/title.png">
</p>

<p align="center">
	<a href="https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ?color=brightgreen&label=Latest&style=for-the-badge"></a>
	<img alt="C++20" src="https://img.shields.io/badge/C%2B%2B-20-00599C.svg?logo=cplusplus&logoColor=white&style=for-the-badge">
	<img alt="Windows x64" src="https://img.shields.io/badge/Windows-x64-0078D6.svg?logo=windows&logoColor=white&style=for-the-badge">
	<a href="LICENSE"><img alt="License: GPLv3" src="https://img.shields.io/badge/License-GPLv3-red.svg?style=for-the-badge"></a>
	<a href="https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/stargazers"><img alt="Stars" src="https://img.shields.io/github/stars/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ?style=for-the-badge"></a>
</p>
<p align="center">
  <strong>简体中文</strong>
  ·
  <a href="https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ">GitHub</a>
</p>

---

NCM OSP 是面向 **VirtualDJ** 的网易云音乐在线源插件。  
它把日推、喜欢、歌单、播客、专辑、视频和搜索直接接到 VirtualDJ 的 **Online Sources** 里，让你在混音台里像浏览本地曲库一样浏览网易云。

当前版本 **260905 v0.3.0** 已不再依赖独立 Python 控制面板：插件本体会自动拉起本地 API、内置深色设置窗口、扫码登录，并在 VirtualDJ 退出时一并关掉后台服务。

> 使用本项目时请遵守当地法律法规，并尊重网易云音乐的服务条款。本插件仅供个人学习与研究，不提供破解、绕过会员或批量抓取能力。

<img alt="NCM OSP Hero" src="docs/img/hero.png" />

## Features ✨

- **在线曲库：日推、我喜欢、创建 / 收藏歌单**
- **播客与声音：创建 / 收藏电台，支持标题自动拆分**
- **收藏专辑、收藏视频，并单独列出 MV**
- **搜索单曲 / 声音 / 视频 / MV，也支持直接粘贴网易云链接**
- 内置深色设置窗口，高度集成进 VirtualDJ
- 启动时自动拉起本地 API，退出时自动结束
- 扫码登录，头像、昵称、用户 ID 就地显示
- 播放音质可选 MP3 / FLAC，视频可选 720P / 1080P
- 右键下载曲目或视频，可写入 Tag 与封面
- 超大列表自动切分，封面尺寸可调
- 右键即可打开设置、配置、日志、安装目录和下载目录

<div align="center">
	<img height="420px" src="docs/img/settings.png" alt="内置设置窗口">
</div>

## Online library 🎵

登录后，在 VirtualDJ 左侧 **网络曲库 / Online Sources** 展开 `NeteaseCloudMusic`，即可按区块浏览：

| 区块 | 说明 |
| --- | --- |
| 每日歌曲推荐 | 当天日推，服务就绪且有数据时才显示 |
| 我喜欢的音乐 | 账号红心列表 |
| 我创建的歌单 / 我收藏的歌单 | 可分别开关，并限制个数 |
| 我创建的播客 / 我收藏的播客 | 电台节目，支持标题拆分 |
| 我收藏的专辑 | 显示为 `专辑名 - 艺人` |
| 我收藏的视频 | 再拆成 **全部视频** 与 **MV** |

超大歌单、播客、专辑、视频列表可按「单个列表项数上限」自动切成 `名称-1`、`名称-2`…，避免一次塞进上千条把界面拖死。

<div align="center">
	<img height="280px" src="docs/img/library.png" alt="VirtualDJ 中的网易云曲库">
	<img height="280px" src="docs/img/covers.png" alt="带封面的曲目列表">
</div>

## Search, links and media 🔍

搜索框同时承担两件事：

1. **关键词搜索**  
   在设置里选定一种类型：单曲、声音、视频或 MV。插件会按「搜索返回项数上限」自动翻页合并结果。
2. **粘贴网易云链接**  
   识别 `music.163.com` 的单曲、节目、电台、歌单、专辑、MV、视频（含 `/#/`、分享参数、`outchain`）。识别成功后不再走关键词搜索。

加载到 Deck 时才向 API 要直链：

- 歌曲 / 声音：`/song/url/v1`，音质 `exhigh`（MP3）或 `lossless`（FLAC）
- 官方 MV：`/mv/url`
- 云视频：`/video/url`

没有对应权限时会自动降级，而不是硬失败。

<div align="center">
	<img height="240px" src="docs/img/search.png" alt="搜索单曲 / 声音 / 视频 / MV">
	<img height="240px" src="docs/img/link-paste.png" alt="粘贴网易云链接">
	<img height="240px" src="docs/img/video.png" alt="视频与 MV 播放">
</div>

## Login and settings 🔐

设置窗口是插件自己的 Win32 深色界面，可从两处打开：

- VirtualDJ 插件界面
- 在 `NeteaseCloudMusic` 根目录右键 → **打开插件设置**

**账户信息**
- 显示头像、昵称、用户 ID
- **登录** 会打开本地二维码页，扫码成功后写回 `ncm_user_data.json`
- **退出登录** 同时清 Cookie、头像缓存，并请求 API `/logout`

**内容与数量**
- 八类内容开关：日推、喜欢、创建/收藏歌单、创建/收藏播客、收藏专辑、收藏视频
- 列表个数、单表项数、搜索条数上限（1–999）
- 封面尺寸上限（音频和视频共用 `?param=NxN`）

**搜索 / 音画质 / 下载 / 其他 / 服务**
- 搜索类别四选一
- 播放音质 MP3 / FLAC，视频 1080P / 720P
- 曲目下载、视频下载分开开关；可写 Tag、封面、源链接
- 自动拆分声音标题、自动切分超大列表
- 日志、API 端口、CMD 窗口、在线状态、一键重启 API

配置即时写入插件目录下的 `settings.json`。改端口或「显示 CMD 窗口」后点 **重启 API 服务** 才会生效。

<div align="center">
	<img height="260px" src="docs/img/login.png" alt="扫码登录">
	<img height="260px" src="docs/img/qr-browser.png" alt="浏览器二维码页">
</div>

## Download and tags 💾

在设置里打开对应下载开关后，曲目或视频右键会出现：

- **下载此曲目**
- **下载此视频**

后台线程拉取直链，保存到设定目录（默认「下载」文件夹下的 `NeteaseCloudMusic DL`）：

| 类型 | 文件 | 备注 |
| --- | --- | --- |
| 普通歌曲 | `艺人 - 标题.mp3` / `.flac` | FLAC 仅无损开启时 |
| 播客 / 声音 | `作者 - 标题.mp3` | 可按 ` - ` 拆标题 |
| 视频 / MV | `创作者 - 标题.mp4` | 720P / 1080P，自动降级 |

开启「写入曲目信息至 Tag」后，会用 TagLib 写入：

- MP3：ID3v2 标题 / 艺人，可选 Comment 源链接，APIC 封面
- FLAC：Vorbis 标签 + FLAC Picture 封面

下载没有弹窗进度，去保存路径或看 `log.log` 即可。

<div align="center">
	<img height="240px" src="docs/img/context-track.png" alt="曲目右键下载">
	<img height="240px" src="docs/img/download-folder.png" alt="本地下载目录">
</div>

## Context menu 📂

在插件根目录右键：

| 菜单 | 行为 |
| --- | --- |
| By 小小小小铭 / 260905 v0.3.0 | 打开 GitHub 与 Bilibili |
| 打开插件设置 | 弹出内置设置窗 |
| 打开配置文件 | 记事本打开 `settings.json` |
| 打开日志文件 | 记事本打开 `log.log` |
| 打开插件安装目录 | 资源管理器 |
| 打开曲目下载目录 | 资源管理器 |

<div align="center">
	<img height="280px" src="docs/img/context-root.png" alt="插件根目录右键菜单">
</div>

## How to install 📥

### 环境

- Windows x64
- [VirtualDJ](https://cn.virtualdj.com/) **Pro**（Online Sources 需要 Pro 授权）
- 推荐 VirtualDJ 2025

### 发行包安装

1. 到 [Releases](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/releases/latest) 下载最新包
2. 解压后把这些文件放到同一目录：

```
VirtualDJ\Plugins64\OnlineSources\
├─ NeteaseCloudMusic.dll
├─ ncm_api_server.exe
└─ （首次运行后自动生成）
    ├─ settings.json
    ├─ ncm_user_data.json
    ├─ ncm_user_avatar.jpg
    └─ log.log
```

常见插件目录：

- `%LOCALAPPDATA%\VirtualDJ\Plugins64\OnlineSources`
- 或非系统盘 `X:\VirtualDJ\Plugins64\OnlineSources`

3. 启动 VirtualDJ，展开 **Online Sources → NeteaseCloudMusic**
4. 打开插件设置，点 **登录**，用网易云 App 扫码
5. 在搜索齿轮里勾选 **NeteaseCloudMusic**，即可搜索

升级时请先退出 VirtualDJ，覆盖 `dll` 与 `ncm_api_server.exe`。`settings.json` 与登录数据可保留；若行为异常，再删掉它们重新登录。

### 从源码编译

仓库：[SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ)

```
├─ Plugin/                            当前插件源码 260905 v0.3.0（NeteaseCloudMusic.dll）
├─ API Server/api-enhanced/           对 api-enhanced 的 VirtualDJ 补丁（不是完整上游仓库）
├─ VirtualDJ_OnlineSource.slnx
├─ docs/img/                          README 配图
└─ Legacy/
   ├─ v0.1.0/                         260331 v0.1
   └─ v0.2.0/                         260420 v0.2
```

- Visual Studio 2022 / 2026，工具集 `v145`，C++20
- 配置 `Release | x64`
- 静态链接：libcurl、jsoncpp、TagLib、zlib
- 系统库：`ws2_32` `crypt32` `dwmapi` `uxtheme` `windowscodecs` 等
- 输出为 `NeteaseCloudMusic.dll`，导出 `DllGetClassObject`

API 侧请先使用 [NeteaseCloudMusicAPI Enhanced](https://github.com/neteasecloudmusicapienhanced/api-enhanced)，再按 [`API Server/README.md`](API%20Server/README.md) 覆盖本仓库中的修改文件。历史发行版源码在 [`Legacy/v0.1.0`](Legacy/v0.1.0) 与 [`Legacy/v0.2.0`](Legacy/v0.2.0)。

## Architecture 🧩

```
VirtualDJ
   │  IVdjPluginOnlineSource
   ▼
NeteaseCloudMusic.dll          Win32 深色设置窗 / 扫码登录 / 搜索 / 直链 / 下载
   │  http://127.0.0.1:{port}
   ▼
ncm_api_server.exe             NeteaseCloudMusicAPI Enhanced
   │  Job Object（VDJ 退出即杀）
   ▼
网易云音乐
```

插件加载时 `CreateProcess` 拉起 API，并把子进程放进 `KILL_ON_JOB_CLOSE` 作业对象；`Release()` 或 VirtualDJ 异常退出时，API 都不会残留。状态栏的 Online / Offline 只看进程是否还活着，不会拿 `/login/status` 轮询刷日志。

## FAQ ❓

**插件在 VirtualDJ 里不出现？**  
确认是 Windows x64 + Pro，文件在 `Plugins64\OnlineSources`，且 64 位 VirtualDJ。

**API 显示 Offline？**  
看同目录有没有 `ncm_api_server.exe`，端口是否被占。改端口后点「重启 API 服务」。

**扫不出码 / 登不上？**  
先等状态变成 Online，再登录。浏览器能打开二维码页就说明服务正常。

**没有 320kbps / 无损 / 高清视频？**  
取决于账号权限。插件只请求设定的最高规格，没有就自动降级。

**歌单显示不全？**  
把「单个列表项数上限」调高，并打开「自动切分超大列表」。上游接口对单次拉取仍有限制。

**下载没有反应？**  
下载是后台进行的。去保存路径或 `log.log` 查看；未开对应下载开关时右键不会出现菜单。

## Contributing 💖

欢迎 Issue、讨论和 Pull Request。反馈可走：

- [GitHub Issues](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/issues)
- [Bilibili](https://space.bilibili.com/475951038)

请附上 `log.log`、VirtualDJ 版本、Windows 版本和复现步骤。不要在 Issue 里贴完整 Cookie。

## Credits 🙌

- [小小小小铭 / DJM1NG](https://space.bilibili.com/475951038) — 作者
- [NeteaseCloudMusicAPI Enhanced](https://github.com/neteasecloudmusicapienhanced/api-enhanced) — 本地 API
- [VirtualDJ Plugin SDK](https://cn.virtualdj.com/wiki/Developers.html) — Online Source 接口
- 所有提出问题和试用的用户

### Dependencies

- [libcurl](https://curl.se/libcurl/)
- [JsonCpp](https://github.com/open-source-parsers/jsoncpp)
- [TagLib](https://taglib.org/)（MP3 ID3v2 / FLAC Picture）
- [zlib](https://zlib.net/)
- Windows：Win32、DWM Dark Mode、WIC、Job Object

## Legacy 🗃️

- [`Legacy/v0.1.0`](Legacy/v0.1.0)：260331 v0.1
- [`Legacy/v0.2.0`](Legacy/v0.2.0)：260420 v0.2（独立 Python / PySide6 控制面板 + C++ 插件）

那两版都需要先开控制面板再启动 VirtualDJ。当前 v0.3.0 已把登录、设置和 API 生命周期收进 DLL。

## License 📄

[GNU General Public License v3.0](LICENSE)

---

<p align="center">
	<a href="https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ">GitHub</a>
	·
	<a href="https://space.bilibili.com/475951038">Bilibili</a>
	·
	<a href="https://cn.virtualdj.com/">VirtualDJ</a>
</p>

# 开发相关

## 代码结构

```
Plugin/NeteaseCloudMusic/
├─ main.cpp
├─ NeteaseCloudMusic.h
├─ NeteaseCloudMusic.cpp
├─ SettingsWindow.cpp
├─ NeteaseCloudMusic.def
├─ vdjOnlineSource.h
└─ VdjPlugin8.h
```

| 文件 | 做什么 |
| --- | --- |
| `main.cpp` | 导出 `DllGetClassObject`。VirtualDJ 用 `CLSID_VdjPlugin8` + `IID_IVdjPluginOnlineSource` 来要对象，这里 `new CNeteaseCloud()` |
| `NeteaseCloudMusic.h` | `PluginConfig` 和 `CNeteaseCloud` 声明 |
| `NeteaseCloudMusic.cpp` | HTTP、歌单、搜索、`GetStreamUrl`、下载、写 Tag |
| `SettingsWindow.cpp` | Win32 深色设置窗、扫码登录、`CreateProcess` 拉起 / 结束 `ncm_api_server.exe` |
| `NeteaseCloudMusic.def` | 导出 `DllGetClassObject` |
| `vdjOnlineSource.h` / `VdjPlugin8.h` | VirtualDJ Plugin SDK |

`CNeteaseCloud` 继承 `IVdjPluginOnlineSource`。和浏览、播放直接相关的回调：

| 回调 | 时机 |
| --- | --- |
| `OnLoad` | 读 `settings.json`、读登录数据、拉起 API |
| `GetFolderList` | 展开 `NeteaseCloudMusic` 根目录 |
| `GetFolder` | 点进某个列表 |
| `OnSearch` / `OnSearchCancel` | 搜索框回车 / 取消 |
| `GetStreamUrl` | 拖到 Deck |
| `GetContextMenu` | 曲目右键下载 |
| `GetFolderContextMenu` | 根目录右键（设置、日志等） |
| `Release` | 关掉 API |

本地 API 源码不在插件工程里。DLL 只负责启动同目录的 `ncm_api_server.exe`，请求打到 `http://127.0.0.1:{port}`。Cookie 存在 `ncm_user_data.json`。

---

## 部署

环境：Windows x64、Visual Studio 2022 / 2026、工具集 `v145`、C++20。配置用 **Release | x64**。静态链接 libcurl、jsoncpp、TagLib、zlib。

1. 打开 `Plugin/NeteaseCloudMusic/NeteaseCloudMusic.vcxproj`（本地若仍用根目录工程，打开自己的 slnx 即可）
2. 编出 `NeteaseCloudMusic.dll`
3. 把 `ncm_api_server.exe` 放到 DLL 同一目录  
   - 现成文件：[`API Server/api-enhanced/precompiled/ncm_api_server.exe`](../../API%20Server/api-enhanced/precompiled/ncm_api_server.exe)  
   - 或克隆 [api-enhanced](https://github.com/neteasecloudmusicapienhanced/api-enhanced)，按 [`API Server/README.md`](../../API%20Server/README.md) 覆盖 `server.js` 和扫码页后再打包
4. `dll` 和 `exe` 一起放到 VirtualDJ 的 `Plugins64\OnlineSources`
5. 启动 VirtualDJ，**网络曲库**里应出现 `NeteaseCloudMusic`

没有 exe 时插件能加载，但登录、列表、播放都会失败。改端口或「显示 CMD 窗口」后要点设置里的 **重启 API**。

---

## 歌单 / 音视频在 VirtualDJ 里怎么加载

整条链路是「先注册元数据，后取直链」。VirtualDJ 不会在刷列表时就要播放地址。

**1. 注册列表**

展开根目录时走 `GetFolderList()`。插件用当前 uid 请求 API，再 `subList->add(folderId, 显示名)`。没有数据的项不注册，避免点进去是空的。

常见 folderId：

- `NCM_DAILY_RECOMMEND_LIST`
- `NCM_NORMAL_LIST_{歌单id}`，超大时带 `_PART_n`
- `NCM_PODCAST_LIST_{电台id}`
- 专辑、视频列表同样用前缀 + 源 id

分隔行（「我创建的歌单」这类）也是一条 folderId，点进去会直接 `finish()`，不填曲目。

**2. 填充曲目**

点进列表走 `GetFolder(folderId)`。按 id 前缀决定请求：

- 日推：`/recommend/songs`
- 歌单：`/playlist/track/all`
- 播客：电台节目列表
- 专辑：`/album`
- 视频：`/mv/sublist`，再拆成全部视频 / MV

然后 `AddTrack` / `AddVideoTrack` 调 `tracksList->add(...)`。填完必须 `tracksList->finish()`。超大列表按 `limitItemsPerFolder` 切块，只加载当前 part。

**3. 播放**

拖到 Deck 才走 `GetStreamUrl(uniqueId)`：

| uniqueId 前缀 | 取流 |
| --- | --- |
| `NCM_NORMAL_TRACK_` | `/song/url/v1?id=...&level=exhigh\|lossless` |
| `NCM_PODCAST_TRACK_` | 节目 id → `/dj/program/detail` 换成 `mainSong.id`，再 `/song/url/v1` |
| `NCM_MUSIC_VIDEO_` | `/mv/url?id=...&r=1080\|720` |
| `NCM_NORMAL_VIDEO_` | `/video/url?id=...&res=1080\|720` |

返回值赋给 SDK 的 `url`，之后由 VirtualDJ 拉这条 HTTP 地址。没对应权限时接口会降级，插件把降级后的地址照样交回去；彻底没有则 `S_FALSE`。

**4. 搜索**

`OnSearch` 先 `HandleSearchLink()`。输入里如果有 `music.163.com` 的 `/song` `/playlist` `/album` `/mv` `/video` 等路径，就按链接解析，不再做关键词搜索。否则按设置里的 type（单曲 1 / 声音 2000 / 视频 1014 / MV 1004）分页请求，合并到 `searchResultLimit`。搜索结果同样只填元数据，播放还是 `GetStreamUrl`。取消搜索时 `OnSearchCancel` 置位，翻页循环会提前停。

---

## 注册曲目时值得注意的细节

音频 / 播客走 `AddTrack()`，视频走 `AddVideoTrack()`。参数含义以源码注释为准：

```cpp
// 曲目添加基准
void CNeteaseCloud::AddTrack(IVdjTracksList* list, const Json::Value& data, bool isPC) {
    string id, name, ar, pic;
    if (isPC) {
        // 播客：使用 NCM_PODCAST_TRACK_ 前缀 + 节目ID
        id = "NCM_PODCAST_TRACK_" + data["id"].asString();

        // --- 增加播客解析逻辑 ---
        string rawName = data["name"].asString();
        string rawAr = data["dj"]["nickname"].asString();

        if (config.splitPodcastTitle) {               //也执行切分逻辑
            size_t pos = rawName.find(" - ");
            if (pos != string::npos) {
                ar = rawName.substr(0, pos);
                name = rawName.substr(pos + 3);
            }
            else {
                name = rawName;
                ar = rawAr;
            }
        }
        else {
            name = rawName;
            ar = rawAr;
        }
        // ------------------------

        pic = PodcastCoverFromProgram(data);
    }
    else {
        // 普通：使用 NCM_NORMAL_TRACK_ 前缀 + 歌曲ID
        id = "NCM_NORMAL_TRACK_" + data["id"].asString();
        name = data["name"].asString();
        for (auto& a : data["ar"]) {
            if (!ar.empty()) ar += ",";
            ar += a["name"].asString();
        }
        pic = data["al"]["picUrl"].asString();
    }

    if (pic.find("//") == 0) pic = "https:" + pic;
    pic = SizedCover(pic, config.coverSize);   // 曲目/播客封面应用 ?param=NxN

    list->add(
        id.c_str(),    // 1. 唯一 ID：该曲目在 VDJ 数据库中的标识
        name.c_str(),  // 2. 标题：对应 VDJ 界面标题列
        ar.c_str(),    // 3. 艺人：对应 VDJ 界面艺人列
        0,             // 4. 混音/版本名，空
        0,             // 5. 风格，空
        0,             // 6. 厂牌，空
        0,             // 7. 备注，空
        pic.c_str(),   // 8. 封面链接
        0,             // 9. 直链（留空，加载时再走 GetStreamUrl）
        0.0f           // 10. 时长（秒，0 表示让 VDJ 加载时自行计算）
    );
}

void CNeteaseCloud::AddVideoTrack(IVdjTracksList* list, const Json::Value& data) {
    const string videoId = data["vid"].asString();
    if (videoId.empty()) return;

    // 收藏接口把“本站 MV”标成 type 0（纯数字 id）；其余条目都是云视频
    // （hash id），需走 /video/url。
    const bool isMv = data.get("type", -1).asInt() == 0;
    const string id = string(isMv ? "NCM_MUSIC_VIDEO_" : "NCM_NORMAL_VIDEO_") + videoId;
    const string title = data.get("title", "").asString();
    string creator;
    for (const auto& item : data["creator"]) {
        if (!creator.empty()) creator += ",";
        creator += item.get("userName", "").asString();
    }
    if (creator.empty()) creator = "网易云音乐视频";

    string cover = data.get("coverUrl", "").asString();
    if (cover.find("//") == 0) cover = "https:" + cover;
    cover = SizedCover(cover, config.coverSize);
    const float durationSeconds = (float)data.get("durationms", 0).asInt() / 1000.0f;
    // IVdjTracksList::add 把时长当作第 10 个（下标 10）参数。把本条标成视频，
    // 好让 VirtualDJ 把返回的 MP4 流当作视频媒体播放。
    list->add(id.c_str(), title.c_str(), creator.c_str(),
        nullptr, nullptr, nullptr, nullptr, cover.c_str(), nullptr,
        durationSeconds, 0.0f, 0, 0, true);
}
```

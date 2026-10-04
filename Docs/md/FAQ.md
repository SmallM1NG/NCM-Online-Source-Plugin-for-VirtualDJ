# FAQ ❓

**1.** <b style="font-size: 1.15em">Q:</b> <b style="font-size: 1.15em">插件放入对应位置后在 VirtualDJ 中看不到？</b>

&emsp;**A:** 本插件仅支持 **Windows x64** 版本的 VirtualDJ，需拥有 **VirtualDJ Pro** 许可证才可使用（这是 VirtualDJ 的硬性要求）

&emsp;请确认文件放在数据目录的 `Plugins64\OnlineSources` 里，且能看到 `NeteaseCloudMusic.dll` 和 `ncm_api_server.exe`。2025 之前和之后的数据目录不一样，改过自定义目录的请放到你自己的目录里。

**2.** <b style="font-size: 1.15em">Q:</b> <b style="font-size: 1.15em">获取不到指定音质？</b>

&emsp;**A:** 请确保你拥有对应 VIP 等级。音质均为可获得的上限，如没有则自动降级返回。凭据过期时也会降级，可退出登录后重新扫码。

**3.** <b style="font-size: 1.15em">Q:</b> <b style="font-size: 1.15em">获取不到完整曲目？</b>

&emsp;**A:** 请确保你拥有对应 VIP 等级，尝试重新登录。网易云侧无版权或账号权限不够时，列表里可能看得到，实际取流会失败。

**4.** <b style="font-size: 1.15em">Q:</b> <b style="font-size: 1.15em">曲目在列表能看到但是播放显示错误？</b>

&emsp;**A:** 可能是网易云那里没有此音源（灰色状态）。

**5.** <b style="font-size: 1.15em">Q:</b> <b style="font-size: 1.15em">插件启动不了？</b>

&emsp;**A:** 请确保 API 服务的 exe **没有改文件名**，并且和 dll 放在同一目录。尝试更换端口并重启 API。

**6.** <b style="font-size: 1.15em">Q:</b> <b style="font-size: 1.15em">每次都要重新登录吗？</b>

&emsp;**A:** 不用。只有获取不到高音质 / 完整内容时，才是凭据过期，再退出登录重新扫码。

**7.** <b style="font-size: 1.15em">Q:</b> <b style="font-size: 1.15em">改了内容 / 数量设置，列表没变？</b>

&emsp;**A:** 这些配置改完后，点击 `NeteaseCloudMusic` 折叠再展开，才会重新拉列表。API 默认有 2 分钟缓存，同一个请求短期内可能还是旧数据。

**8.** <b style="font-size: 1.15em">Q:</b> <b style="font-size: 1.15em">搜索不到内容？</b>

&emsp;**A:** 先点浏览窗输入框旁的小齿轮，勾选 `NeteaseCloudMusic`。

**9.** <b style="font-size: 1.15em">Q:</b> <b style="font-size: 1.15em">搜索返回条目？</b>

&emsp;**A:** 插件支持根据设定的搜索类别返回指定类别条目，也支持粘贴部分链接，直接获取链接内容。

| 类型 | 路径 |
| --- | --- |
| 单曲 | `/song` |
| 节目 / 声音 | `/program`、`/dj` |
| 电台 / 播客 | `/djradio`、`/radio` |
| 歌单 / 榜单 | `/playlist`、`/toplist`、`/my/m/playlist` |
| 专辑 | `/album` |
| MV | `/mv` |
| 视频 / mlog | `/video`、`/mlog` |

&emsp;搜索有自己的独立条目上限，返回多少条目会根据这个上限进行输出。链接的内容也遵循这个上限；如果你粘贴了专辑和播客等链接，它们的条目也会根据这个上限截断处理。

**10.** <b style="font-size: 1.15em">Q:</b> <b style="font-size: 1.15em">为什么上限是 999？</b>

&emsp;**A:** 因为 VirtualDJ 支持单个列表中项的上限就是 999。不建议把列表中项上限设得太高，这样可能会导致卡顿。你可以使用超大列表切分功能，插件会自动按照设定的上限值把大歌单切成 `xxx-1`、`xxx-2` 这样的子歌单。如果没有启用，并且列表内的条目超出上限，则超出上限的条目都不会显示。

**11.** <b style="font-size: 1.15em">Q:</b> <b style="font-size: 1.15em">列表排序不对？</b>

&emsp;**A:** 请点击浏览窗列表表头左上角空白区域，确认是按照这个地方排序（有箭头提示），然后点击 `NeteaseCloudMusic` 折叠列表再重新展开即可。请不要使用别的排序方式，这样会打乱列表顺序。

**12.** <b style="font-size: 1.15em">Q:</b> <b style="font-size: 1.15em">封面没有自动下载？</b>

&emsp;**A:** 你可以在 VirtualDJ 设置中搜索 `cover`，找到 `coverDownload`，改成 `always` 即可。

**13.** <b style="font-size: 1.15em">Q:</b> <b style="font-size: 1.15em">如何取消封面自动下载？</b>

&emsp;**A:** 你可以在 VirtualDJ 设置中搜索 `cover`，找到 `coverDownload`，改成 `no` 即可。

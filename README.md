🎵 NCM Online Source Plugin for VirtualDJ 使用教程
ℹ️ 插件信息 (Info)
开发者：小小小小铭 Aka DJM1NG

开源协议：GPLv3

构建日期：20260331

兼容性：仅兼容 Windows x64。

推荐环境：VirtualDJ 2025版（需拥有 Pro 许可证）。

相关链接：GitHub 仓库 | Bilibili 主页

⚠️ 步骤 0：安全备份 (Step 0)
虽然此步骤可忽略，但强烈建议您备份您的数据库文件！
通常 VirtualDJ 的曲目数据位于：

系统盘 (C盘)：C:\Users\用户名\AppData\Local\VirtualDJ

非系统盘：对应盘符根目录下的 \VirtualDJ 文件夹（例如 D:\VirtualDJ）。

🚀 步骤 1：启动服务 (Step 1)
完全解压压缩包内的内容到同一个文件夹下。

双击运行 Control Panel（控制面板）。

点击顶部按钮 “启用服务”，等待状态显示为 “运行中”。

可点击界面上的服务地址（如 http://localhost:3000）验证是否成功启动。

🔑 步骤 2：账号登录 (Step 2)
在登录界面选择登录方式：

二维码登录：使用网易云音乐手机 App 扫码。

手机号登录：输入手机号及验证码。

授权成功后，若界面能够加载出您的头像和用户信息，即代表登录成功。

⚙️ 步骤 3：生成配置 (Step 3)
在控制面板依次点击：

生成 Cookie

生成 UserData

点击 插件偏好设置，根据需求调整音质、下载路径等，点击 生成当前配置并保存。

上述文件将自动保存在程序目录下的 \export 文件夹内。

💾 步骤 4：安装插件 (Step 4)
启动 VirtualDJ。

进入设置界面，点击 右下角小齿轮 图标打开 VirtualDJ 主目录。

定位到目录：\Plugins64\OnlineSources。

将 \export 文件夹内的 3个 .txt 文件 以及解压包里的 NeteaseCloudMusic.dll 复制到该目录下。

🎚️ 步骤 5：启用插件 (Step 5)
重启 VirtualDJ。

在浏览窗左侧找到 网络曲库 (Online Sources) 分类。

点击展开，找到 NeteaseCloudMusic 目录并点击即可启用。

所有的歌单将以子文件夹形式展示，您可以直接将曲目拖入 Deck 播放。

提示：右键点击该目录可查看版本、日志及配置文件。

🔍 步骤 6：搜索与下载 (Step 6)
开启搜索：点击浏览窗搜索框旁的小齿轮，启用网络曲库搜索，并确保源已勾选 NeteaseCloudMusic。

检索曲目：选中 NeteaseCloudMusic 目录后，直接在搜索框键入关键词即可在线检索。

下载功能：确保在偏好设置中启用了 下载功能 并设置了有效路径。右键点击歌单内的曲目，选择 下载此歌曲 即可。

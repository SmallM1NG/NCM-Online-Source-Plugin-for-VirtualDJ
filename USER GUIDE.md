# 🎵 控制面板使用教程

## ℹ️ INFO

* **开发者**: 小小小小铭 Aka DJM1NG (遵循 **GPLv3** 协议)
* **版本**: 260420 v0.2
* **支持平台**: **Windows x64** | **推荐使用**: **VirtualDJ 2025 (需 Pro 授权)**
* **相关链接**: [GitHub 仓库](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ) | [Bilibili 主页](https://space.bilibili.com/475951038)

> 使用本插件请务必遵守相关法律法规，尊重网易云音乐服务条款。

[![](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/blob/main/img/1.png)](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/blob/main/img/1.png)

---

## ⚠️ Step 0: 数据备份

虽然此步骤可忽略 **但强烈建议您备份您的数据库文件！**
通常 VirtualDJ 的曲目数据位于：

* **系统盘 (C盘)**: `C:\Users\username\AppData\Local\VirtualDJ`
* **非系统盘**: 对应盘符根目录下的 `\VirtualDJ` 文件夹（例如 `D:\VirtualDJ` 或 `E:\VirtualDJ`）

[![](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/blob/main/img/2.png)](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/blob/main/img/2.png)

---

## 🚀 Step 1: 启动服务

1. 确保已解压全部内容到同一文件夹下。
2. 点击顶部 **[启动服务]** 按钮，等待地址旁状态显示为 **运行中**。
3. 启动后可点击 **[服务地址]**，如弹出状态页则代表服务正常工作。

[![](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/blob/main/img/3.png)](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/blob/main/img/3.png)

---

## 🔑 Step 2: 账户登录

1. 在登录区域选择 **APP 扫码** 或 **手机验证码** 方式登录。
2. 按照对应的操作流程授权登录。
3. 授权成功后，若能加载您的 **用户头像昵称等**，即代表登录成功。

[![](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/blob/main/img/4.png)](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/blob/main/img/4.png)

---

## ⚙️ Step 3: 生成文件

1. 依次点击 **[生成 Cookie]** 与 **[生成 UserData]**。这是您的用户凭证，插件需要这些凭证才能正常调用您的数据。
2. 点击 **[插件偏好设置]**，在弹出的子界面可以调整插件的一些设置，对应项旁边会有详细说明，调整好点击保存设置。

* 本版本已实现读取 VDJ 数据路径，所有文件自动写入至您的目录，无需手动复制文件，插件也会在首次运行时自动检查并安装。
* 如果自动读取的路径错误，请点击路径右侧齿轮手动指定（注意：指定的路径不包括 `\Plugins64\OnlineSources`）。
* 不需要每次使用插件都重新生成 Cookie 和 UserData。
* 如您发现无法获取到 **完整歌曲或高音质格式** 等，则代表信息过期，此时才需要重新登录并生成。

[![](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/blob/main/img/5.png)](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/blob/main/img/5.png)

---

## 🎚️ Step 4: 启用插件

1. 启动 **VirtualDJ**。
2. 在左侧浏览窗找到 **网络曲库 (Online Sources)** 分类点击展开。
3. 找到 **NeteaseCloudMusic** 目录并点击，即可加载歌单数据。

[![](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/blob/main/img/7.png)](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/blob/main/img/6.png)

---

## 🔍 Step 5: 搜索与下载

* **搜索**: 点击搜索框旁的小齿轮，确保搜索源已勾选 **NeteaseCloudMusic**，可按图示进行配置。
* **下载**: 偏好设置内启用下载功能后，在歌单内右键点击曲目，选择 **[下载此歌曲]** 即可开始下载。
* 下载操作不会弹出提示，请前往预设的下载路径查看文件。
* 下载后的文件会自动根据偏好设置项写入 Tag 信息。

[![](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/blob/main/img/8.png)](https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ/blob/main/img/7.png)

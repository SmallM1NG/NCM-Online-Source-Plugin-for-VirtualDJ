import sys
import os
import subprocess
import json
import time
import requests
import base64
from PySide6.QtWidgets import (QApplication, QMainWindow, QPushButton, QVBoxLayout,
                               QHBoxLayout, QWidget, QLabel, QTextEdit, QComboBox,
                               QLineEdit, QStackedWidget, QDialog, QCheckBox, QFormLayout, QFileDialog, QScrollArea,
                               QMessageBox, QToolTip, QMenu)
from PySide6.QtCore import Qt, QTimer, QRegularExpression, QThread, Signal, QSize, QRect, QPoint
from PySide6.QtGui import QPixmap, QIcon, QRegularExpressionValidator, QFont, QGuiApplication, QIntValidator, QCursor


# --- 动态获取当前系统的下载目录 ---
def get_default_download_path():
    home = os.path.expanduser("~")
    return os.path.join(home, "Downloads", "NeteaseCloudMusic DL")


# --- FAQ 窗口类 ---
class FAQDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("常见问题解答 (FAQ)")
        self.resize(500, 600)  # 调大初始尺寸以匹配大字体
        self.setMinimumSize(400, 500)
        self.center()

        layout = QVBoxLayout(self)

        # 使用滚动区域防止内容过多溢出
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        # 移除滚动区域边框，看起来更现代
        scroll.setStyleSheet("QScrollArea { border: none; background: transparent; }")

        content = QWidget()
        v_box = QVBoxLayout(content)
        v_box.setSpacing(20)  # 每个问题之间的间距

        faq_data = [
            ("❓ 服务无法启动？",
             "请检查后台是否已经有服务在运行（app.exe）。如有请使用任务管理器结束或重启电脑。如没有请检查 3000 端口是否被占用 如被占用请在偏好设置中手动更改空闲端口。"),
            ("❓ 插件在 VirtualDJ 里没显示？",
             "仅兼容Windows x64 版 VirtualDJ 需拥有 Pro 许可证, 如上述符合请检查插件安装位置和文件内容 或尝试重新下载安装"),
            ("❓ 二维码加载不出来？",
             "请确保服务已正确启动，可点击主界面的服务地址验证。若浏览器能打开，则尝试刷新二维码或切换手机号登录。"),
            ("❓ 无法获取 320Kbps 或无损音质？",
             "请确保登录的账号拥有对应的 VIP 权限，并重新生成 Cookie 和 UserData。"),
            ("❓ 导出文件是必须的吗？",
             "是的！Cookie 和 UserData 是插件读取账号状态的唯一凭据，settings 则是你的个性化配置。"),
            ("❓ 如何下载歌曲？",
             "在“偏好设置”中开启下载功能并保存，重启 VirtualDJ 后，在歌曲上右键即可看到下载选项。"),
            ("❓ 歌单内曲目显示不全？",
             "API限制 对单个歌单的曲目加载上限通常为 1000 首，播客则为500首，建议将大歌单拆分。"),
            ("❓ 调试与 Bug 反馈？", "请在GitHub反馈或B站私信我。"),
        ]

        for title, text in faq_data:
            item_container = QWidget()
            item_layout = QVBoxLayout(item_container)
            item_layout.setContentsMargins(5, 5, 5, 5)

            # 标题：加大、加粗、颜色稍作区分（但不写死，确保在黑白模式下都可见）
            t_lbl = QLabel(title)
            t_lbl.setWordWrap(True)
            t_lbl.setStyleSheet("""
                font-weight: bold; 
                font-size: 18px; 
                color: #0078d7; 
            """)

            # 回答：加大字号，设为 16px
            content_lbl = QLabel(text)
            content_lbl.setWordWrap(True)
            content_lbl.setStyleSheet("font-size: 16px; line-height: 140%;")

            item_layout.addWidget(t_lbl)
            item_layout.addWidget(content_lbl)
            v_box.addWidget(item_container)

        v_box.addStretch()
        scroll.setWidget(content)
        layout.addWidget(scroll)

        # 关闭按钮
        btn_close = QPushButton("我知道了")
        btn_close.setMinimumHeight(40)
        btn_close.setStyleSheet("font-size: 14px; font-weight: bold;")
        btn_close.clicked.connect(self.accept)
        layout.addWidget(btn_close)

    def center(self):
        # 获取主屏幕的对象
        screen = QGuiApplication.primaryScreen()
        # 获取屏幕的可用几何尺寸（排除任务栏）
        screen_geometry = screen.availableGeometry()
        # 获取窗口自身的几何尺寸
        window_geometry = self.frameGeometry()

        # 计算中心点
        center_point = screen_geometry.center()
        # 将窗口几何体的中心移动到屏幕中心点
        window_geometry.moveCenter(center_point)

        # 将窗口实际移动到计算出的左上角坐标
        self.move(window_geometry.topLeft())


# --- 使用教程窗口类 ---
class TutorialDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("使用教程")
        self.resize(900, 720)
        self.setMinimumSize(600, 480)
        self.current_step = 0

        self.steps_text = [
            # 第一页：Info
            (
                "<b>Info</b><br><br>"
                "本插件由 <b>小小小小铭 Aka DJM1NG</b> 开发 遵循 <b>GPLv3</b> 协议<br> "
                "版本: <b>260331 v0.1</b> <a href='https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ'>GitHub仓库</a> "
                "<a href='https://space.bilibili.com/475951038'>Bilibili主页</a><br>"
                "本插件仅兼容 <b>Windows x64 VirtualDJ 推荐使用25版 需拥有'Pro'许可证</b> <br>"
                "使用本插件时请务必遵守相关法律法规 尊重网易云音乐的服务条款"
            ),

            # 第二页：Step 0
            (
                "<b>Step 0</b><br><br>"
                "⚠️ <b>注意：</b><br>"
                "虽然此步骤可忽略 <span style='color: #FF0000;'>但强烈建议您备份您的数据库文件 ！</span><br>"
                "通常C盘的曲目数据会在VDJ主目录下 例 " + r"C:\Users\username\AppData\Local\VirtualDJ<br>"
                "非C盘则在对应盘符下的根目录 例 " + r"D:\VirtualDJ " + r"E:\VirtualDJ"
            ),

            # 第三页：登录
            (
                "<b>Step 1🚀</b><br><br>"
                "完全解压压缩包内的内容到同文件夹下<br>"
                "双击运行 <b>Control Panel</b> 点击顶部按钮启用服务,等待显示'运行中'<br>"
                "可点击服务地址验证启动状态<br>"
            ),

            (
                "<b>Step 2🔑</b><br><br>"
                "点击选择栏选择登录方式 可选网易云APP扫码登录 或 手机号+验证码登录 授权登陆后如可加载用户信息即代表登陆成功<br>"
            ),
            (
                "<b>Step 3⚙️</b><br><br>"
                "依次点击 <b>生成Cookie</b> <b>生成UserData</b><br> "
                "点击 <b>插件偏好设置</b> 进入设置调整子界面 调整好后点 <b>生成当前配置并保存</b> 保存偏好设置<br>"
                "以上文件均会保存在 <b>程序运行目录下</b> " + r"<b>\export</b>" + "文件夹内<br> "
            ),
            (
                "<b>Step 4💾</b><br><br>"
                "启动VirtualDJ 点击设置按钮进入设置主界面 点击 <b>右下角小齿轮</b> 打开VirtualDJ主目录<br> "
                "在此目录下找到 " + r"<b>\Plugins64\OnlineSources</b>""<br>"
                "复制上述 " + r"<b>\export</b>" + " 内 <b>3个.txt文件</b> 到此目录<br> "
                "复制文件夹内 <b>NeteaseCloudMusic.dll</b> 文件 到此目录<br> "

            ),
            (
                "<b>Step 5🎚️</b><br><br>"
                "重启VirtualDJ 在 <b>浏览窗左侧</b> 找到 <b>网络曲库</b> 分类 点击展开 找到 <b>NeteaseCloudMusic</b>目录 并点击即可启用插件<br> "
                "全部类歌单都将以 <b>NeteaseCloudMusic</b> 目录下的子文件夹展示<br>"
                "点击歌单 选中曲目即可拖入Deck播放<br>"
                "右键点击 <b>NeteaseCloudMusic</b> 目录可查看构建日期 配置文件 日志文件等<br>"
            ),
            (
                "<b>Step 6🔍️</b><br><br>"
                "点击 浏览窗搜索框旁小齿轮 如图示在VirtualDJ内启用网络曲库搜索 并配置源为<b>NeteaseCloudMusic</b><br> "
                "点击 <b>NeteaseCloudMusic</b> 目录 此时浏览窗应显示 <b>通过检索框检索曲目</b> 在搜索框中键入信息即可搜索<br> "
                "确保配置文件中启用了 <b>下载功能</b> 并设置好保存路径 右键歌单内曲目 点击 <b>下载此歌曲</b> 即可下载 下载不会有任何提示<br>"
            ),
        ]

        self.init_ui()
        self.update_page()

    def init_ui(self):
        layout = QVBoxLayout(self)
        self.stack = QStackedWidget()

        for i in range(8):
            page = QWidget()
            page_layout = QVBoxLayout(page)

            # 文字标签
            lbl = QLabel(self.steps_text[i])
            lbl.setWordWrap(True)
            lbl.setOpenExternalLinks(True)
            lbl.setTextInteractionFlags(Qt.TextInteractionFlag.TextBrowserInteraction)
            lbl.setStyleSheet("font-size: 20px; padding: 10px; background: transparent;")
            #lbl.setMaximumHeight(180)  # 限制文字高度，给图片留空间

            # 图片标签
            img_lbl = QLabel()
            img_lbl.setObjectName(f"img_label_{i}")  # 给标签起名，方便后面定位
            img_lbl.setAlignment(Qt.AlignCenter)
            # 💡 注意：这里取消了 setScaledContents，改为手动代码缩放
            img_lbl.setStyleSheet("border: 1px solid rgba(128, 128, 128, 50);")

            page_layout.addWidget(lbl, 1)
            page_layout.addWidget(img_lbl, 5)  # 图片占大头
            self.stack.addWidget(page)

        layout.addWidget(self.stack)

        # 底部信息和按钮 (保持你原有的逻辑)
        self.page_info = QLabel()
        self.page_info.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.page_info)

        btn_layout = QHBoxLayout()
        self.btn_faq = QPushButton("❓ FAQ");
        self.btn_faq.clicked.connect(self.open_faq)
        self.btn_prev = QPushButton("上一页");
        self.btn_prev.clicked.connect(self.prev_page)
        self.btn_next = QPushButton("下一页");
        self.btn_next.clicked.connect(self.next_page)
        self.btn_close = QPushButton("我知道了");
        self.btn_close.clicked.connect(self.accept)
        self.btn_close.setStyleSheet("background-color: #0078d7; color: white; font-weight: bold;")
        btn_layout.addWidget(self.btn_faq);
        btn_layout.addStretch()
        btn_layout.addWidget(self.btn_prev);
        btn_layout.addWidget(self.btn_next);
        btn_layout.addWidget(self.btn_close)
        layout.addLayout(btn_layout)

    def update_page(self):
        """刷新当前页面的内容和图片"""
        self.stack.setCurrentIndex(self.current_step)
        self.page_info.setText(f"步骤 {self.current_step + 1} / 8")
        self.btn_prev.setEnabled(self.current_step > 0)
        is_last = (self.current_step == 7)
        self.btn_next.setVisible(not is_last)
        self.btn_close.setVisible(is_last)

        # 核心：根据当前标签的实际尺寸缩放图片
        current_page = self.stack.currentWidget()
        img_label = current_page.findChild(QLabel, f"img_label_{self.current_step}")

        if img_label:
            self.load_scaled_img(img_label, self.current_step + 1)

    def load_scaled_img(self, label, img_idx):
        """通用的图片缩放加载函数"""
        for ext in [".png", ".jpg", ".jpeg"]:
            img_path = os.path.join(os.getcwd(), "img", f"{img_idx}{ext}")
            if os.path.exists(img_path):
                pix = QPixmap(img_path)
                if not pix.isNull():
                    # 关键点：KeepAspectRatio 保证不拉伸变形，SmoothTransformation 保证缩放不模糊
                    # 减去 20 像素的 padding 留白
                    target_size = label.size() - QSize(20, 20)
                    if target_size.width() > 0 and target_size.height() > 0:
                        scaled_pix = pix.scaled(target_size, Qt.KeepAspectRatio, Qt.SmoothTransformation)
                        label.setPixmap(scaled_pix)
                break

    def resizeEvent(self, event):
        """监听窗口大小改变事件，实时调整图片"""
        super().resizeEvent(event)
        # 窗口大小变了，重新刷一下当前页面的图片
        self.update_page()

    def showEvent(self, event):
        """窗口显示时居中并刷新"""
        super().showEvent(event)
        self.center()
        self.update_page()

    def next_page(self):
        if self.current_step < 7:
            self.current_step += 1
            self.update_page()

    def prev_page(self):
        if self.current_step > 0:
            self.current_step -= 1
            self.update_page()

    def center(self):
        screen = QGuiApplication.primaryScreen().availableGeometry()
        size = self.geometry()
        self.move((screen.width() - size.width()) // 2, (screen.height() - size.height()) // 2)

    def open_faq(self):
        FAQDialog(self).show()





# --- 后台检测服务线程 ---
class HealthCheckThread(QThread):
    status_signal = Signal(bool)
    runtime_signal = Signal(str)

    def __init__(self, main_win):
        super().__init__()
        self.main_win = main_win

    def run(self):
        while True:
            is_running = False
            port = self.main_win.get_current_port() # 实时获取
            try:
                res = requests.get(f"http://localhost:{port}/banner", timeout=1.0)
                is_running = (res.status_code == 200)
                self.status_signal.emit(is_running)
            except:
                self.status_signal.emit(False)

            if is_running and self.main_win.service_start_time:
                delta = int(time.time() - self.main_win.service_start_time)
                hours, remainder = divmod(delta, 3600)
                minutes, seconds = divmod(remainder, 60)
                self.runtime_signal.emit(f"{hours:02}:{minutes:02}:{seconds:02}")
            else:
                self.runtime_signal.emit("00:00:00")
            self.sleep(1)


# --- 偏好设置窗口类 ---
class SettingsDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("插件偏好设置")
        self.resize(420, 750)  # 稍微调高以容纳新增的端口项
        self.setMinimumSize(420, 650)

        self.export_dir = os.path.join(os.getcwd(), "export")
        self.settings_file = os.path.join(self.export_dir, "settings.txt")

        self.init_ui()
        self.load_config_from_export()

    def init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QScrollArea.NoFrame)

        container = QWidget()
        self.form = QFormLayout(container)
        self.form.setContentsMargins(20, 15, 20, 20)
        self.form.setSpacing(8)
        self.form.setLabelAlignment(Qt.AlignLeft)
        self.form.setFormAlignment(Qt.AlignLeft | Qt.AlignTop)

        header_style = "font-weight: bold; color: #0078d7; font-size: 14px;"

        # 💡 弹出提示菜单逻辑：点击显示，点其他地方或再次点击关闭
        def show_help(btn, text):
            menu = QMenu(self)
            #menu.setStyleSheet("QMenu { border: 1px solid #ccc; padding: 5px; background: white; }")
            for line in text.split('\n'):
                act = menu.addAction(line)
                act.setEnabled(False)
            menu.exec(btn.mapToGlobal(QPoint(0, btn.height())))

        def add_section_header(text, help_text):
            h_layout = QHBoxLayout()
            lbl = QLabel(f"<b>{text}</b>")
            lbl.setStyleSheet(header_style)

            help_btn = QPushButton("❓")
            help_btn.setFixedSize(25, 25)
            help_btn.setFlat(True)
            help_btn.setCursor(Qt.PointingHandCursor)
            help_btn.clicked.connect(lambda: show_help(help_btn, help_text))

            h_layout.addWidget(lbl)
            h_layout.addWidget(help_btn)
            h_layout.addStretch()
            self.form.addRow(h_layout)

        # --- 1. 基础开关 ---
        add_section_header("--- 基础开关 ---", "控制加载的歌单类型\n勾选即加载")
        self.cb_daily = QCheckBox();
        self.cb_like = QCheckBox()
        self.cb_mine = QCheckBox();
        self.cb_collect = QCheckBox()
        self.cb_mine_podcast = QCheckBox();
        self.cb_collect_podcast = QCheckBox()

        self.form.addRow("加载每日歌曲推荐", self.cb_daily)
        self.form.addRow("加载我喜欢的音乐", self.cb_like)
        self.form.addRow("加载我创建的歌单", self.cb_mine)
        self.form.addRow("加载我收藏的歌单", self.cb_collect)
        self.form.addRow("加载我创建的播客", self.cb_mine_podcast)
        self.form.addRow("加载我收藏的播客", self.cb_collect_podcast)

        # --- 2. 数量上限 ---
        add_section_header("--- 数量上限 ---",
                           "为防止卡顿请酌情设限\n仅可填入 1 - 1000 值\n"
                           "曲目展示上限为单个歌单内可显示的曲目数量上限\n"
                           "由于API限制 歌单内曲目仅可返回至多1000首 播客为500首")

        def set_strict_limit(edit_obj, max_val):
            edit_obj.setMaxLength(len(str(max_val)))

            def on_text_changed(text):
                if text == "0": edit_obj.setText("1"); return
                if text and int(text) > max_val:
                    edit_obj.setText(str(max_val))
                elif len(text) > 1 and text.startswith("0"):
                    edit_obj.setText(str(int(text)))

            def on_editing_finished():
                val = edit_obj.text().strip()
                if not val or int(val) < 1: edit_obj.setText("1")

            edit_obj.textChanged.connect(on_text_changed)
            edit_obj.editingFinished.connect(on_editing_finished)

        self.edit_mine_limit = QLineEdit();
        self.edit_collect_limit = QLineEdit()
        self.edit_mine_podcast_limit = QLineEdit();
        self.edit_collect_podcast_limit = QLineEdit()
        self.edit_track_limit = QLineEdit();
        self.edit_search_limit = QLineEdit()

        set_strict_limit(self.edit_mine_limit, 1000)
        set_strict_limit(self.edit_collect_limit, 1000)
        set_strict_limit(self.edit_mine_podcast_limit, 1000)
        set_strict_limit(self.edit_collect_podcast_limit, 1000)
        set_strict_limit(self.edit_track_limit, 1000)
        set_strict_limit(self.edit_search_limit, 100)

        self.form.addRow("我创建的歌单上限", self.edit_mine_limit)
        self.form.addRow("我收藏的歌单上限", self.edit_collect_limit)
        self.form.addRow("我创建的播客上限", self.edit_mine_podcast_limit)
        self.form.addRow("我收藏的播客上限", self.edit_collect_podcast_limit)
        self.form.addRow("歌单内曲目展示上限", self.edit_track_limit)
        self.form.addRow("搜索返回曲目数量上限", self.edit_search_limit)

        # --- 3. 音质与下载 ---
        add_section_header("--- 音质与下载 ---",
                           "音质类可选 flac/mp3 即为无损/极高 (mp3码率为320Kbps)\n"
                           "!如果曲目没有此处设定的音质等级 则默认返回降级音频流!\n"
                           "!且正常曲目可以下载flac 但播客内容仅可下载-mp3- 即使下载音质设为flac!\n"
                           "启用写入Tag信息会在下载曲目时将对应的 曲名 作者 封面 写入至曲目Tag\n"
                           "当 启用写入Tag信息 时 启用写入InfoTag信息会将 曲目ID 链接 等写入Tag的备注类")
        self.combo_play_quality = QComboBox();
        self.combo_play_quality.addItems(["mp3", "flac"])
        self.cb_enable_download = QCheckBox()
        self.combo_download_quality = QComboBox();
        self.combo_download_quality.addItems(["mp3", "flac"])
        self.cb_write_tag = QCheckBox();
        self.cb_write_info_tag = QCheckBox()

        self.edit_path = QLineEdit()
        btn_browse = QPushButton("浏览...");
        btn_browse.setFixedWidth(60)
        btn_browse.clicked.connect(self.browse_folder)
        path_layout = QHBoxLayout();
        path_layout.addWidget(self.edit_path);
        path_layout.addWidget(btn_browse)

        self.form.addRow("播放音质", self.combo_play_quality)
        self.form.addRow("启用下载功能", self.cb_enable_download)
        self.form.addRow("下载音质", self.combo_download_quality)
        self.form.addRow("保存路径", path_layout)
        self.form.addRow("写入Tag信息", self.cb_write_tag)
        self.form.addRow("写入InfoTag信息", self.cb_write_info_tag)

        # --- 4. Debug ---
        add_section_header("--- Debug ---", "保持默认即可 如果3000端口被占用可手动指定端口\n1 - 65535")

        # 💡 新增端口输入框
        self.edit_port = QLineEdit()
        self.edit_port.setPlaceholderText("默认 3000")
        set_strict_limit(self.edit_port, 65535)  # 复用限制函数

        self.cb_log = QCheckBox()
        self.form.addRow("启用日志输出", self.cb_log)
        self.form.addRow("API端口设置", self.edit_port)

        scroll.setWidget(container)
        layout.addWidget(scroll)

        footer_layout = QHBoxLayout()
        footer_layout.setContentsMargins(15, 10, 15, 15)
        btn_reset = QPushButton("恢复默认配置")
        btn_reset.setMinimumHeight(35)
        btn_reset.clicked.connect(self.reset_to_default)

        btn_gen = QPushButton("生成当前配置并保存")
        btn_gen.setMinimumHeight(35)
        btn_gen.setStyleSheet("background-color: #0078d7; color: white; font-weight: bold;")
        btn_gen.clicked.connect(self.save_and_overwrite)

        footer_layout.addWidget(btn_reset);
        footer_layout.addWidget(btn_gen)
        layout.addLayout(footer_layout)

    def browse_folder(self):
        path = QFileDialog.getExistingDirectory(self, "选择下载保存目录")
        if path: self.edit_path.setText(path.replace("/", "\\"))

    def get_default_dict(self):
        return {
            "加载每日歌曲推荐": "false", "加载我喜欢的音乐": "true", "加载我创建的歌单": "true",
            "加载我收藏的歌单": "true", "加载我创建的播客": "false", "加载我收藏的播客": "false",
            "我创建的歌单上限": "1000", "我收藏的歌单上限": "1000", "我创建的播客上限": "1000",
            "我收藏的播客上限": "1000", "歌单内曲目展示上限": "1000", "搜索返回曲目数量上限": "30",
            "播放音质": "mp3", "启用下载功能": "false", "下载音质": "mp3",
            "保存路径": get_default_download_path(),
            "写入Tag信息": "true", "写入InfoTag信息": "false",
            "启用日志输出": "true", "API端口设置": "3000",
        }

    def reset_to_default(self):
        reply = QMessageBox.question(self, "确认重置", "确定要将所有配置恢复为默认值吗？",
                                     QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply == QMessageBox.Yes: self._apply_to_ui(self.get_default_dict())

    def load_config_from_export(self):
        config = self.get_default_dict()
        if os.path.exists(self.settings_file):
            try:
                with open(self.settings_file, "r", encoding="utf-8") as f:
                    for line in f:
                        line = line.strip()
                        if "=" in line and not line.startswith("#"):
                            k, v = line.split("=", 1)
                            if k in config: config[k] = v
            except:
                pass
        self._apply_to_ui(config)

    def _apply_to_ui(self, config):
        self.cb_daily.setChecked(config.get("加载每日歌曲推荐") == "true")
        self.cb_like.setChecked(config.get("加载我喜欢的音乐") == "true")
        self.cb_mine.setChecked(config.get("加载我创建的歌单") == "true")
        self.cb_collect.setChecked(config.get("加载我收藏的歌单") == "true")
        self.cb_mine_podcast.setChecked(config.get("加载我创建的播客") == "true")
        self.cb_collect_podcast.setChecked(config.get("加载我收藏的播客") == "true")
        self.edit_mine_limit.setText(config.get("我创建的歌单上限", "1000"))
        self.edit_collect_limit.setText(config.get("我收藏的歌单上限", "1000"))
        self.edit_mine_podcast_limit.setText(config.get("我创建的播客上限", "1000"))
        self.edit_collect_podcast_limit.setText(config.get("我收藏的播客上限", "1000"))
        self.edit_track_limit.setText(config.get("歌单内曲目展示上限", "1000"))
        self.edit_search_limit.setText(config.get("搜索返回曲目数量上限", "30"))
        self.combo_play_quality.setCurrentText(config.get("播放音质", "mp3"))
        self.cb_enable_download.setChecked(config.get("启用下载功能") == "true")
        self.combo_download_quality.setCurrentText(config.get("下载音质", "mp3"))
        self.edit_path.setText(config.get("保存路径", get_default_download_path()))
        self.cb_write_tag.setChecked(config.get("写入Tag信息") == "true")
        self.cb_write_info_tag.setChecked(config.get("写入InfoTag信息") == "true")
        self.cb_log.setChecked(config.get("启用日志输出") == "true")
        self.edit_port.setText(config.get("API端口设置", "3000"))

    def save_and_overwrite(self):
        os.makedirs(self.export_dir, exist_ok=True)
        with open(self.settings_file, "w", encoding="utf-8") as f:
            f.write(self._get_config_string())
        if self.parent(): self.parent().write_log(f"✅ 偏好设置已保存")
        self.accept()

    def _get_config_string(self):
        return f"""# --- 基础开关  ---
# 仅可填入 true 或 false
加载每日歌曲推荐={str(self.cb_daily.isChecked()).lower()}
加载我喜欢的音乐={str(self.cb_like.isChecked()).lower()}
加载我创建的歌单={str(self.cb_mine.isChecked()).lower()}
加载我收藏的歌单={str(self.cb_collect.isChecked()).lower()}
加载我创建的播客={str(self.cb_mine_podcast.isChecked()).lower()}
加载我收藏的播客={str(self.cb_collect_podcast.isChecked()).lower()}

# --- 数量上限 ---
# 为防止卡顿请酌情设限
# 仅可填入 1 - 1000 值 
# xxx歌单/播客上限是指显示该类型歌单数量
# 曲目展示上限为单个歌单内可显示的曲目数量上限
# 由于API限制 歌单内曲目仅可返回至多1000首 播客为500首
# 搜索返回曲目建议 1 - 100 首
我创建的歌单上限={self.edit_mine_limit.text()}
我收藏的歌单上限={self.edit_collect_limit.text()}
我创建的播客上限={self.edit_mine_podcast_limit.text()}
我收藏的播客上限={self.edit_collect_podcast_limit.text()}
歌单内曲目展示上限={self.edit_track_limit.text()}
搜索返回曲目数量上限={self.edit_search_limit.text()}

# --- 音质与下载 ---
# 音质类可选 flac/mp3 即为无损/极高 (mp3码率为320Kbps)
# !如果曲目没有此处设定的音质等级 则默认返回降级音频流!
# !且正常曲目可以下载flac 但播客内容仅可下载-mp3- 即使下载音质设为flac!
# 启用写入Tag信息会在下载曲目时将对应的 曲名 作者 封面 写入至曲目Tag
# 当 启用写入Tag信息 时 启用写入InfoTag信息会将 曲目ID 链接 等写入Tag的备注类
播放音质={self.combo_play_quality.currentText()}
启用下载功能={str(self.cb_enable_download.isChecked()).lower()}
下载音质={self.combo_download_quality.currentText()}
保存路径={self.edit_path.text()}
写入Tag信息={str(self.cb_write_tag.isChecked()).lower()}
写入InfoTag信息={str(self.cb_write_info_tag.isChecked()).lower()}

# --- Debug ---
# 保持默认即可 如果3000端口被占用可手动指定端口 1-65535
启用日志输出={str(self.cb_log.isChecked()).lower()}
API端口设置={self.edit_port.text()}"""


# --- 主窗口类 ---
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Control Panel Version: 260331 v0.1")

        icon_path = os.path.join(os.getcwd(), "img", "ico.ico")
        if os.path.exists(icon_path): self.setWindowIcon(QIcon(icon_path))

        self.api_process = None
        self.current_cookie = ""
        self.qr_key = ""
        self.service_start_time = None
        self.is_first_run = True

        self.load_ui_config()
        self.init_ui()

        self.health_thread = HealthCheckThread(self)
        self.health_thread.status_signal.connect(self.update_service_status_ui)
        self.health_thread.runtime_signal.connect(self.update_runtime_ui)
        self.health_thread.start()

        self.qr_check_timer = QTimer()
        self.qr_check_timer.timeout.connect(self.poll_qr_status)

        if self.is_first_run:
            QTimer.singleShot(800, self.show_tutorial)

    def resource_path(self, relative_path):
        if hasattr(sys, '_MEIPASS'):
            return os.path.join(sys._MEIPASS, relative_path)
        return os.path.join(os.path.abspath("."), relative_path)

    def get_current_port(self):
        """实时从 settings.txt 读取端口"""
        settings_path = os.path.join(os.getcwd(), "export", "settings.txt")
        if os.path.exists(settings_path):
            try:
                with open(settings_path, "r", encoding="utf-8") as f:
                    for line in f:
                        if "API端口设置=" in line:
                            return line.split("=")[1].strip()
            except:
                pass
        return "3000"

    def init_ui(self):
        central = QWidget();
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)
        self.resize(500, 700);
        self.setMinimumSize(400, 700)

        # 1. 服务控制
        service_row = QHBoxLayout()
        self.btn_toggle_service = QPushButton("启动服务")
        self.btn_toggle_service.clicked.connect(self.toggle_service)
        self.runtime_label = QLabel("已运行: 00:00:00")
        self.runtime_label.setStyleSheet("color: #555; font-family: 'Consolas'; font-weight: bold;")
        service_row.addWidget(self.btn_toggle_service);
        service_row.addStretch();
        service_row.addWidget(self.runtime_label)
        layout.addLayout(service_row)

        # 2. 动态地址栏
        self.addr_label = QLabel()
        self.refresh_addr_display(False)  # 初始状态
        self.addr_label.setOpenExternalLinks(True)
        self.addr_label.setStyleSheet("color: #0078d7; font-weight: bold;")
        layout.addWidget(self.addr_label)

        # 3. 登录区域
        self.login_mode_box = QComboBox();
        self.login_mode_box.addItems(["二维码登录", "手机号登录"])
        self.login_mode_box.currentIndexChanged.connect(lambda i: self.login_stack.setCurrentIndex(i))
        layout.addWidget(self.login_mode_box)

        self.login_stack = QStackedWidget()
        qr_page = QWidget();
        qr_l = QVBoxLayout(qr_page)
        self.qr_img_label = QLabel("等待获取二维码");
        self.qr_img_label.setAlignment(Qt.AlignCenter);
        self.qr_img_label.setFixedSize(180, 180)
        btn_get_qr = QPushButton("获取二维码");
        btn_get_qr.clicked.connect(self.get_qr_code)
        qr_l.addWidget(self.qr_img_label, alignment=Qt.AlignCenter);
        qr_l.addWidget(btn_get_qr)
        self.login_stack.addWidget(qr_page)

        ph_page = QWidget();
        ph_l = QVBoxLayout(ph_page)
        self.input_phone = QLineEdit();
        self.input_phone.setPlaceholderText("手机号")
        self.input_code = QLineEdit();
        self.input_code.setPlaceholderText("验证码")
        btn_send = QPushButton("发送验证码");
        btn_send.clicked.connect(self.send_sms)
        btn_login = QPushButton("登录");
        btn_login.clicked.connect(self.phone_login)
        ph_l.addWidget(self.input_phone);
        ph_l.addWidget(btn_send);
        ph_l.addWidget(self.input_code);
        ph_l.addWidget(btn_login)
        self.login_stack.addWidget(ph_page)
        layout.addWidget(self.login_stack)

        # 4. 用户信息
        self.user_info_widget = QWidget();
        user_info_layout = QHBoxLayout(self.user_info_widget)
        self.avatar_label = QLabel();
        self.avatar_label.setFixedSize(60, 60);
        self.avatar_label.setScaledContents(True);
        self.avatar_label.setVisible(False)
        self.user_detail_label = QLabel("用户信息: 未登录\nID: -")
        user_info_layout.addWidget(self.avatar_label);
        user_info_layout.addWidget(self.user_detail_label)
        layout.addWidget(self.user_info_widget)

        # 5. 功能键
        btn_layout = QHBoxLayout()
        b_cookie = QPushButton("🍪 生成 Cookie");
        b_cookie.clicked.connect(self.export_cookie)
        b_userdata = QPushButton("🗎 生成 UserData");
        b_userdata.clicked.connect(self.export_user_data)
        b_settings = QPushButton("⚙ 插件偏好设置");
        b_settings.clicked.connect(self.open_settings)
        b_tutorial = QPushButton("💡 使用教程");
        b_tutorial.clicked.connect(self.show_tutorial)
        btn_layout.addWidget(b_cookie);
        btn_layout.addWidget(b_userdata);
        btn_layout.addWidget(b_settings);
        btn_layout.addWidget(b_tutorial)
        layout.addLayout(btn_layout)

        self.log_box = QTextEdit();
        self.log_box.setReadOnly(True)
        layout.addWidget(self.log_box)

    def refresh_addr_display(self, is_running):
        port = self.get_current_port()
        status = "(运行中)" if is_running else "(未就绪)"
        url = f"http://localhost:{port}"
        self.addr_label.setText(f'服务地址: <a href="{url}">{url}</a> {status}')

    def open_settings(self):
        dlg = SettingsDialog(self)
        if dlg.exec():
            self.refresh_addr_display(self.api_process is not None)
            self.write_log("⚙ 配置已更新。若修改了端口，请停止并重新启动服务以生效。")

    def toggle_service(self):
        port = self.get_current_port()
        if self.api_process is None:
            exe_path = os.path.join(os.getcwd(), "app.exe")
            if not os.path.exists(exe_path): self.write_log("❌ 未找到 app.exe"); return

            # 不再使用 startupinfo，允许弹出命令行窗口
            self.api_process = subprocess.Popen([exe_path], env=dict(os.environ, PORT=port))
            self.service_start_time = time.time()
            self.write_log(f"🚀 尝试启动服务 端口 {port} ...")
            self.write_log("✅ 服务已启动")
        else:
            self.stop_service()

    def stop_service(self):
        if self.api_process:
            subprocess.run(f"taskkill /F /T /PID {self.api_process.pid}", shell=True, capture_output=True)
            self.api_process = None
            self.service_start_time = None
            self.write_log("🛑 服务已停止.")
            self.refresh_addr_display(False)

    def update_service_status_ui(self, is_running):
        self.refresh_addr_display(is_running)
        if is_running:
            self.btn_toggle_service.setText("停止服务")
            self.btn_toggle_service.setStyleSheet("background-color: #ff4d4f; color: white;")
        else:
            self.btn_toggle_service.setText("启动服务")
            self.btn_toggle_service.setStyleSheet("")

    def get_qr_code(self):
        port = self.get_current_port()
        try:
            base_url = f"http://localhost:{port}"
            res = requests.get(f"{base_url}/login/qr/key?t={int(time.time())}", timeout=2).json()
            self.qr_key = res['data']['unikey']
            res2 = requests.get(f"{base_url}/login/qr/create?key={self.qr_key}&qrimg=true", timeout=2).json()
            qr_base64 = res2['data']['qrimg'].split(',')[1]
            pix = QPixmap();
            pix.loadFromData(base64.b64decode(qr_base64))
            self.qr_img_label.setPixmap(pix.scaled(180, 180, Qt.KeepAspectRatio, Qt.SmoothTransformation))
            self.qr_check_timer.start(1000)
            self.write_log("📸 二维码已获取，请使用网易云音乐App扫码。")
        except:
            self.write_log("❌ 无法连接服务。")

    def poll_qr_status(self):
        port = self.get_current_port()
        try:
            res = requests.get(f"http://localhost:{port}/login/qr/check?key={self.qr_key}&t={int(time.time())}",
                               timeout=1).json()
            if res['code'] == 803:
                self.qr_check_timer.stop()
                self.current_cookie = res['cookie']
                self.write_log("✅ 扫码登录成功！")
                self.get_user_info()
        except:
            pass

    def get_user_info(self):
        port = self.get_current_port()
        try:
            res = requests.post(f"http://localhost:{port}/login/status", json={"cookie": self.current_cookie},
                                timeout=2).json()
            p = res.get('data', {}).get('profile')
            if p:
                self.user_detail_label.setText(f"用户: {p['nickname']}\nID: {p['userId']}")
                pix = QPixmap();
                pix.loadFromData(requests.get(p['avatarUrl'], timeout=2).content)
                self.avatar_label.setPixmap(pix);
                self.avatar_label.setVisible(True)
        except:
            pass

    def send_sms(self):
        port = self.get_current_port()
        try:
            requests.get(f"http://localhost:{port}/captcha/sent?phone={self.input_phone.text()}", timeout=2)
            self.write_log(f"📲 验证码已发送至: {self.input_phone.text()}")
        except:
            self.write_log("❌ 发送验证码失败")

    def phone_login(self):
        port = self.get_current_port()
        try:
            res = requests.get(
                f"http://localhost:{port}/login/cellphone?phone={self.input_phone.text()}&captcha={self.input_code.text()}",
                timeout=2).json()
            if res.get('code') == 200:
                self.current_cookie = res.get('cookie')
                self.get_user_info()
                self.write_log("✅ 手机号登录成功")
        except:
            self.write_log("❌ 登录失败")

    def export_cookie(self):
        if not self.current_cookie: return self.write_log("⚠ 请先登录")
        os.makedirs("export", exist_ok=True)
        with open("export/cookie.txt", "w", encoding="utf-8") as f: f.write(self.current_cookie)
        self.write_log("💾 Cookie 导出成功")

    def export_user_data(self):
        if not self.current_cookie: return self.write_log("⚠ 请先登录")
        port = self.get_current_port()
        try:
            res = requests.post(f"http://localhost:{port}/login/status", json={"cookie": self.current_cookie},
                                timeout=2).json()
            os.makedirs("export", exist_ok=True)
            with open("export/user_data.txt", "w", encoding="utf-8") as f:
                json.dump(res, f, indent=4, ensure_ascii=False)
            self.write_log("💾 UserData 导出成功")
        except:
            pass

    def show_tutorial(self):
        self.tutorial_win = TutorialDialog(self);
        self.tutorial_win.show()

    def update_runtime_ui(self, time_str):
        self.runtime_label.setText(f"已运行: {time_str}" if self.api_process else "已运行: 00:00:00")

    def write_log(self, text):
        self.log_box.append(f"[{time.strftime('%H:%M:%S')}] {text}")

    def load_ui_config(self):
        if os.path.exists("config.json"):
            try:
                with open("config.json", "r") as f:
                    d = json.load(f)
                    self.is_first_run = d.get('first_run', True)
            except:
                pass

    def closeEvent(self, event):
        try:
            with open("config.json", "w") as f:
                json.dump({"first_run": False}, f)
        except:
            pass
        if hasattr(self, 'health_thread'): self.health_thread.terminate()
        self.stop_service();
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv);
    window = MainWindow();
    window.show();
    sys.exit(app.exec())
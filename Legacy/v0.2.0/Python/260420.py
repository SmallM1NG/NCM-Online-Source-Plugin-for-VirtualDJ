import shutil
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
from PySide6.QtGui import QPixmap, QIcon, QRegularExpressionValidator, QFont, QGuiApplication, QIntValidator, QCursor, \
    QTextCursor
import winreg
from ansi2html import Ansi2HTMLConverter

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
        # 移除滚动区域边框,看起来更现代
        scroll.setStyleSheet("QScrollArea { border: none; background: transparent; }")

        content = QWidget()
        v_box = QVBoxLayout(content)
        v_box.setSpacing(20)  # 每个问题之间的间距

        faq_data = [
            ("❓ 服务无法启动？",
             "请检查后台是否已经有服务在运行（app.exe）.如有请使用任务管理器结束或重启电脑.如没有请检查 3000 端口是否被占用 如被占用请在偏好设置中手动更改空闲端口."),
            ("❓ 插件在 VirtualDJ 里没显示？",
             "仅兼容Windows x64 版 VirtualDJ 需拥有 Pro 许可证, 如上述符合请检查插件安装位置和文件内容 或尝试重新下载安装"),
            ("❓ 二维码加载不出来？",
             "请确保服务已正确启动,可点击主界面的服务地址验证.若浏览器能打开,则尝试刷新二维码或切换手机号登录."),
            ("❓ 无法获取 320Kbps 或无损音质？",
             "请确保登录的账号拥有对应的 VIP 权限,并重新生成 Cookie 和 UserData."),
            ("❓ 导出文件是必须的吗？",
             "是的！Cookie 和 UserData 是插件读取账号状态的唯一凭据,settings 则是你的个性化配置.\n不过不需要每次使用都导出 如您发现无法获取到 完整歌曲或高音质格式 等 则代表 信息过期 此时才需要重新登录并生成"),
            ("❓ 如何下载歌曲？",
             "在“偏好设置”中开启下载功能并保存,重启 VirtualDJ 后,在歌曲上右键即可看到下载选项."),
            ("❓ 歌单内曲目显示不全？",
             "API限制 对单个歌单的曲目加载上限通常为 1000 首,播客则为500首,建议将大歌单拆分."),
            ("❓ 调试与 Bug 反馈？", "请在GitHub反馈或B站私信我."),
        ]

        for title, text in faq_data:
            item_container = QWidget()
            item_layout = QVBoxLayout(item_container)
            item_layout.setContentsMargins(5, 5, 5, 5)

            # 标题：加大、加粗、颜色稍作区分（但不写死,确保在黑白模式下都可见）
            t_lbl = QLabel(title)
            t_lbl.setWordWrap(True)
            t_lbl.setStyleSheet("""
                font-weight: bold; 
                font-size: 18px; 
                color: #0078d7; 
            """)

            # 回答：加大字号,设为 16px
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
        self.resize(1200, 900)
        self.setMinimumSize(600, 480)
        self.current_step = 0

        self.steps_text = [
            # 第一页：Info (索引 0)
            (
                "<div style='line-height: 140%;'>"
                "<b>INFO</b><br>"
                "开发者: <b>小小小小铭 Aka DJM1NG</b> (遵循 <b>GPLv3</b> 协议)<br>"
                "版本: <b>260420 v0.2</b> <a href='https://github.com/SmallM1NG/NCM-Online-Source-Plugin-for-VirtualDJ'>GitHub仓库</a> "
                "<a href='https://space.bilibili.com/475951038'>Bilibili主页</a><br>"
                "支持平台: <b>Windows x64</b> | 推荐使用: <b>VirtualDJ 2025 (需 Pro 授权)</b><br><br>"
                "使用本插件请务必遵守相关法律法规,尊重网易云音乐服务条款."
                "</div>"
            ),

            # 第二页：Step 0 (索引 1)
            (
                "<b>Step 0: 数据备份 ⚠️</b><br><br>"
                "虽然此步骤可忽略 <span style='color: #FF0000;'>但强烈建议您备份您的数据库文件 ！</span><br>"
                "通常C盘的曲目数据会在VDJ主目录下 例 " + r"C:\Users\username\AppData\Local\VirtualDJ<br>"
                "非C盘则在对应盘符下的根目录 例 " + r"D:\VirtualDJ " + r"E:\VirtualDJ"
            ),

            # 第三页：Step 1 (索引 2)
            (
                "<b>Step 1: 启动服务 🚀</b><br><br>"
                "1. 确保已解压全部内容到同一文件夹下.<br>"
                "2. 点击顶部<b>[启动服务]</b>按钮,等待地址旁状态显示为 <b>运行中</b>.<br>"
                "3. 启动后可点击<b>[服务地址]</b>,如弹出状态页则代表服务正常工作."
            ),

            # 第四页：Step 2 (索引 3)
            (
                "<b>Step 2: 账户登录 🔑</b><br><br>"
                "1. 在登录区域选择 <b>APP 扫码</b> 或 <b>手机验证码</b> 方式登录.<br>"
                "2. 按照对应的操作流程授权登录<br>"
                "3. 授权成功后,若能加载您的<b>用户头像昵称等</b>,即代表登录成功."

            ),

            # 第五页：Step 3 (索引 4)
            (
                "<b>Step 3: 生成文件 ⚙️</b><br><br>"
                "1. 依次点击<b>[生成 Cookie]</b>与<b>[生成 UserData]</b>.这是您的用户凭证,插件需要这些凭证才能正常调用您的数据<br>"
                "2. 点击<b>[插件偏好设置]</b>,在弹出的子界面可以调整插件的一些设置,对应项旁边会有详细说明,调整好点击保存设置.<br><br>"
                "* 本版本已实现读取VDJ数据路径 所有文件自动写入至您的目录 无需手动复制文件 插件也会在首次运行时自动检查并安装.<br>"
                "* 如果自动读取的路径错误 请点击路径右侧齿轮手动指定<br>"
                "* 注意 指定的路径不包括</i>" + r" \Plugins64\OnlineSources"
                "<br>* 不需要每次使用插件都重新生成Cookie和UserData<br>"
                "* 如您发现无法获取到 完整歌曲或高音质格式 等 则代表 信息过期 此时才需要重新登录并生成."
            ),

            # 第六页：Step 4 (索引 5)
            (
                "<b>Step 4: 启用插件 🎚️</b><br><br>"
                "1. 启动 VirtualDJ.<br>"
                "2. 在左侧浏览窗找到 <b>网络曲库 (Online Sources)</b> 分类点击展开.<br>"
                "3. 找到 <b>NeteaseCloudMusic</b> 目录并点击,即可加载歌单数据."
            ),

            # 第七页：Step 5 (索引 6)
            (
                "<b>Step 5: 搜索与下载 🔍</b><br><br>"
                "• <b>搜索</b>: 点击搜索框旁的小齿轮,确保搜索源已勾选 <b>NeteaseCloudMusic</b>,可按图示进行配置.<br>"
                "• <b>下载</b>: 偏好设置内启用下载功能后,在歌单内右键点击曲目,选择<b>[下载此歌曲]</b>即可开始下载.<br><br>"
                "* 下载操作不会弹出提示,请前往预设的下载路径查看文件.<br>"
                "* 下载后的文件会自动根据偏好设置项写入Tag信息."
            ),
        ]

        self.init_ui()
        self.update_page()

    def init_ui(self):
        layout = QVBoxLayout(self)
        self.stack = QStackedWidget()

        for i in range(7):
            page = QWidget()
            page_layout = QVBoxLayout(page)

            # 文字标签
            lbl = QLabel(self.steps_text[i])
            lbl.setWordWrap(True)
            lbl.setOpenExternalLinks(True)
            lbl.setTextInteractionFlags(Qt.TextInteractionFlag.TextBrowserInteraction)
            # 调大字号以匹配大窗口
            lbl.setStyleSheet("font-size: 18px; padding: 10px; background: transparent;")

            # 图片标签
            img_lbl = QLabel()
            img_lbl.setObjectName(f"img_label_{i}")
            img_lbl.setAlignment(Qt.AlignCenter)

            # 核心：关闭自动缩放,保持图片原始比例和大小
            img_lbl.setScaledContents(False)
            img_lbl.setStyleSheet("border: 1px solid rgba(128, 128, 128, 50);")

            # 将文字和图片加入布局
            # 保持文字较小高度,图片占据更多空间
            page_layout.addWidget(lbl, 1)
            page_layout.addWidget(img_lbl, 8)
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
        self.page_info.setText(f"步骤 {self.current_step + 1} / 7")
        self.btn_prev.setEnabled(self.current_step > 0)
        is_last = (self.current_step == 6)
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
                    # 关键点：KeepAspectRatio 保证不拉伸变形,SmoothTransformation 保证缩放不模糊
                    # 减去 20 像素的 padding 留白
                    target_size = label.size() - QSize(20, 20)
                    if target_size.width() > 0 and target_size.height() > 0:
                        scaled_pix = pix.scaled(target_size, Qt.KeepAspectRatio, Qt.SmoothTransformation)
                        label.setPixmap(scaled_pix)
                break

    def resizeEvent(self, event):
        """监听窗口大小改变事件,实时调整图片"""
        super().resizeEvent(event)
        # 窗口大小变了,重新刷一下当前页面的图片
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



class ProcessLogThread(QThread):
    log_signal = Signal(str)

    def __init__(self, process):
        super().__init__()
        self.process = process

    def run(self):
        # 实时读取进程的 stdout
        while True:
            line = self.process.stdout.readline()
            if line:
                # 解码并发送给主界面显示
                self.log_signal.emit(line.decode('utf-8', errors='ignore').strip())
            if self.process.poll() is not None:
                break





# --- 偏好设置窗口类 ---
class SettingsDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("插件偏好设置")
        self.resize(420, 750)  # 稍微调高以容纳新增的端口项
        self.setMinimumSize(420, 650)

        # self.export_dir = os.path.join(os.getcwd(), "export")
        # self.settings_file = os.path.join(self.export_dir, "settings.txt")

        self.init_ui()
        self.load_config()

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

        # 💡 弹出提示菜单逻辑：点击显示,点其他地方或再次点击关闭
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
                           "当 启用写入Tag信息 时 启用写入InfoTag信息会将 曲目ID 链接 等写入Tag的备注类\n"
                           "播客标题自动切分是指 例如获取到的曲目名称格式为 AAA(作者) - BBB(曲目名称)时 自动解析并变更Tag内容")

        self.combo_play_quality = QComboBox();
        self.combo_play_quality.addItems(["mp3", "flac"])
        self.cb_enable_download = QCheckBox()
        self.combo_download_quality = QComboBox();
        self.combo_download_quality.addItems(["mp3", "flac"])
        self.cb_write_tag = QCheckBox();
        self.cb_write_info_tag = QCheckBox()
        self.cb_split_title = QCheckBox()

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
        self.form.addRow("播客标题自动切分", self.cb_split_title)


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
            "写入Tag信息": "true", "写入InfoTag信息": "false", "播客标题自动切分": "true",
            "启用日志输出": "true", "API端口设置": "3000",
        }

    def reset_to_default(self):
        reply = QMessageBox.question(self, "确认重置", "确定要将所有配置恢复为默认值吗？",
                                     QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply == QMessageBox.Yes: self._apply_to_ui(self.get_default_dict())

    def load_config(self):
        """逻辑：若 settings.txt 不存在，则使用默认字典填充 UI"""
        target_file = os.path.join(self.parent().get_plugin_path(), "settings.txt")

        if not os.path.exists(target_file):
            # 文件缺失，直接用默认值填充 UI
            default_config = self.get_default_dict()
            self._apply_to_ui(default_config)
            return

        try:
            config = {}
            with open(target_file, "r", encoding="utf-8") as f:
                for line in f:
                    if "=" in line and not line.startswith("#"):
                        k, v = line.strip().split("=", 1)
                        config[k] = v
            self._apply_to_ui(config)
        except Exception as e:
            if hasattr(self.parent(), 'write_log'):
                self.parent().write_log(f"⚠ 读取配置异常: {str(e)}")
            self._apply_to_ui(self.get_default_dict())

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
        self.cb_split_title.setChecked(config.get("播客标题自动切分") == "true")
        self.cb_log.setChecked(config.get("启用日志输出") == "true")
        self.edit_port.setText(config.get("API端口设置", "3000"))

    def save_and_overwrite(self):
        """将当前 UI 的配置直接保存到 VDJ 插件目录下的 settings.txt"""
        try:
            # 1. 获取统一的插件路径
            target_dir = self.parent().get_plugin_path()
            settings_file = os.path.join(target_dir, "settings.txt")

            # 2. 获取格式化后的配置字符串
            config_content = self._get_config_string()

            # 3. 写入文件
            with open(settings_file, "w", encoding="utf-8") as f:
                f.write(config_content)

            # --- 修复点：调用父窗口的 write_log 方法 ---
            if hasattr(self.parent(), 'write_log'):
                self.parent().write_log(f"⚙ 偏好设置已保存: {settings_file} 若修改了端口,请停止并重新启动服务以生效")

            self.accept()

        except Exception as e:
            # --- 修复点：异常处理中的日志调用也需要修改 ---
            if hasattr(self.parent(), 'write_log'):
                self.parent().write_log(f"❌ 保存偏好设置失败: {str(e)}")

            QMessageBox.critical(self, "错误", f"无法写入配置文件: {str(e)}")

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
# 播客标题自动切分是指 例如获取到的曲目名称格式为 AAA(作者) - BBB(曲目名称)时 自动解析并变更Tag内容
播放音质={self.combo_play_quality.currentText()}
启用下载功能={str(self.cb_enable_download.isChecked()).lower()}
下载音质={self.combo_download_quality.currentText()}
保存路径={self.edit_path.text()}
写入Tag信息={str(self.cb_write_tag.isChecked()).lower()}
写入InfoTag信息={str(self.cb_write_info_tag.isChecked()).lower()}
播客标题自动切分={str(self.cb_split_title.isChecked()).lower()}

# --- Debug ---
# 保持默认即可 如果3000端口被占用可手动指定端口 1-65535
启用日志输出={str(self.cb_log.isChecked()).lower()}
API端口设置={self.edit_port.text()}"""


class PathEditDialog(QDialog):
    def __init__(self, home_path, run_path, parent=None):
        super().__init__(parent)
        self.setWindowTitle("路径管理")
        self.setMinimumWidth(450)
        self.main_win = parent  # 保存主窗口引用

        layout = QVBoxLayout(self)
        form_layout = QFormLayout()

        self.home_edit = QLineEdit(home_path)
        self.run_edit = QLineEdit(run_path)
        form_layout.addRow("数据路径 (Home):", self.home_edit)
        form_layout.addRow("安装路径 (Run):", self.run_edit)
        layout.addLayout(form_layout)

        # --- 新增：从注册表读取按钮 ---
        self.btn_from_reg = QPushButton("🔄 从注册表重置路径")
        self.btn_from_reg.setToolTip("重新从 Windows 注册表读取 VirtualDJ 的默认路径")
        self.btn_from_reg.clicked.connect(self.load_from_reg)
        layout.addWidget(self.btn_from_reg)

        # 插件管理按钮组
        tools_layout = QHBoxLayout()
        self.reinstall_btn = QPushButton("🛠️ 重新安装插件")
        self.reinstall_btn.clicked.connect(self.install_plugin_logic)
        self.open_folder_btn = QPushButton("📂 打开插件目录")
        self.open_folder_btn.clicked.connect(self.open_plugin_folder)
        tools_layout.addWidget(self.reinstall_btn)
        tools_layout.addWidget(self.open_folder_btn)
        layout.addLayout(tools_layout)

        # 底部保存/取消
        btn_layout = QHBoxLayout()
        self.save_btn = QPushButton("保存配置")
        self.cancel_btn = QPushButton("取消")
        self.save_btn.clicked.connect(self.accept)
        self.cancel_btn.clicked.connect(self.reject)
        btn_layout.addStretch()
        btn_layout.addWidget(self.save_btn)
        btn_layout.addWidget(self.cancel_btn)
        layout.addLayout(btn_layout)

    def load_from_reg(self):
        """调用主窗口逻辑重新读取注册表"""
        if hasattr(self.main_win, 'get_vdj_registry_paths'):
            reg_home, reg_run = self.main_win.get_vdj_registry_paths()
            if reg_home and reg_run:
                self.home_edit.setText(reg_home)
                self.run_edit.setText(reg_run)
                QMessageBox.information(self, "成功", "已从注册表加载路径，点击'保存'后生效")
            else:
                QMessageBox.warning(self, "错误", "未能识别注册表路径，请手动输入")

    def get_plugin_target_path(self):
        """辅助方法：计算当前的插件目标目录"""
        home_dir = self.home_edit.text().strip()
        return os.path.join(home_dir, "Plugins64", "OnlineSources")

    def install_plugin_logic(self):
        """将程序运行目录下的 .dll 部署到目标目录"""
        source_dll = os.path.join(os.getcwd(), "NeteaseCloudMusic.dll")
        target_dir = self.get_plugin_target_path()
        target_file = os.path.join(target_dir, "NeteaseCloudMusic.dll")

        try:
            # 检查源文件
            if not os.path.exists(source_dll):
                QMessageBox.warning(self, "错误", f"找不到源文件: {source_dll}")
                return

            # 创建目标目录
            os.makedirs(target_dir, exist_ok=True)

            # 复制文件 (shutil.copy2 会保留元数据)
            shutil.copy2(source_dll, target_file)
            QMessageBox.information(self, "成功", f"插件已成功安装至:\n{target_file}")

        except Exception as e:
            QMessageBox.critical(self, "失败", f"安装插件时出错:\n{str(e)}")

    def open_plugin_folder(self):
        """调用系统资源管理器快速打开插件目录"""
        target_dir = self.get_plugin_target_path()

        if os.path.exists(target_dir):
            # Windows 专用：使用 os.startfile 直接打开
            os.startfile(target_dir)
        else:
            # 如果目录不存在，询问是否创建并打开
            res = QMessageBox.question(self, "目录不存在", "插件目录尚未创建，是否现在创建并打开？")
            if res == QMessageBox.StandardButton.Yes:
                os.makedirs(target_dir, exist_ok=True)
                os.startfile(target_dir)

    def get_paths(self):
        """返回当前输入框中的值"""
        return self.home_edit.text().strip(), self.run_edit.text().strip()

# --- 主窗口类 ---
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Control Panel Version: 260420 v0.2")

        icon_path = os.path.join(os.getcwd(), "img", "ico.ico")
        if os.path.exists(icon_path): self.setWindowIcon(QIcon(icon_path))

        self.ansi_converter = Ansi2HTMLConverter(inline=True)

        self.api_process = None
        self.current_cookie = ""
        self.qr_key = ""
        self.service_start_time = None
        self.is_first_run = True

        self.vdj_home = ""
        self.vdj_run = ""
        self.is_first_run = True

        self.init_ui()

        # --- 步骤 2: UI 创建后再加载配置, 此时即使触发 write_log 也是安全的 ---
        self.load_ui_config()


        self.health_thread = HealthCheckThread(self)
        self.health_thread.status_signal.connect(self.update_service_status_ui)
        self.health_thread.runtime_signal.connect(self.update_runtime_ui)
        self.health_thread.start()

        self.qr_check_timer = QTimer()
        self.qr_check_timer.timeout.connect(self.poll_qr_status)

        if self.is_first_run:
            QTimer.singleShot(800, self.show_tutorial)
            self.auto_deploy_plugin()

    def resource_path(self, relative_path):
        if hasattr(sys, '_MEIPASS'):
            return os.path.join(sys._MEIPASS, relative_path)
        return os.path.join(os.path.abspath("."), relative_path)

    def get_current_port(self):

        # 使用我们统一定义的 get_plugin_path 方法获取路径
        settings_path = os.path.join(self.get_plugin_path(), "settings.txt")

        if os.path.exists(settings_path):
            try:
                with open(settings_path, "r", encoding="utf-8") as f:
                    for line in f:
                        # 注意：如果 settings.txt 里的键名改成了 "监听端口",请同步修改此处
                        if "API端口设置=" in line:
                            parts = line.split("=")
                            if len(parts) > 1:
                                port = parts[1].strip()
                                if port:  # 确保端口号不为空
                                    return port
            except Exception as e:
                # 读取失败时在控制台记录,但不打断程序,返回默认值
                print(f"读取端口配置失败: {e}")

        # 如果文件不存在、格式错误或读取异常,默认返回 3000
        return "3000"

    def get_plugin_path(self):
        """统一获取 VDJ 在线资源插件目录"""
        # 优先使用配置的 vdj_home,如果为空则动态获取
        target = os.path.join(self.vdj_home, "Plugins64", "OnlineSources")
        # 只有当父目录（VDJ Home）确实存在时，才创建子目录
        if os.path.exists(self.vdj_home):
            os.makedirs(target, exist_ok=True)
        return target

    def edit_vdj_paths(self):
        dlg = PathEditDialog(self.vdj_home, self.vdj_run, self)
        if dlg.exec():
            new_home, new_run = dlg.get_paths()

            # 1. 更新内存变量
            self.vdj_home = new_home
            self.vdj_run = new_run

            # 2. 关键：手动更新界面标签！
            self.run_path_label.setText(self.format_path(self.vdj_run, "VDJ安装路径"))
            self.home_path_label.setText(self.format_path(self.vdj_home, "VDJ数据路径"))

            # 3. 保存到配置文件
            self.save_config_to_json()


    def save_config_to_json(self):
        """统一的持久化逻辑, 确保路径不丢失"""
        config_data = {
            "first_run": False,
            "vdj_home": self.vdj_home,
            "vdj_run": self.vdj_run
        }
        try:
            with open("config.json", "w", encoding="utf-8") as f:
                json.dump(config_data, f, indent=4, ensure_ascii=False)
            self.write_log("⚙️ 路径配置已保存 config.json")
        except Exception as e:
            # 这里的 e 会被传给 write_log, 如果 log_box 未就绪, 之前的 hasattr 检查会拦截它
            self.write_log(f"❌ 写入 config.json 失败: {str(e)}")

    def format_path(self, path_str, prefix):
        if not path_str or path_str == "":
            return f"{prefix}: 未设置"
        if len(path_str) > 30:
            return f"{prefix}: {path_str[:10]}...{path_str[-15:]}"
        return f"{prefix}: {path_str}"

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
        user_info_layout.setContentsMargins(10, 5, 10, 5)  # 设置边距防止溢出

        # --- 左侧：头像 ---
        self.avatar_label = QLabel()
        self.avatar_label.setFixedSize(50, 50)
        self.avatar_label.setScaledContents(True);
        self.avatar_label.setStyleSheet("background-color: #ddd;")  # 示例样式


        # --- 中间偏左：用户详情 ---
        self.user_detail_label = QLabel("用户名: 未登录\n用户ID: -")
        self.user_detail_label.setStyleSheet("font-weight: bold;")

        self.user_info_widget.setLayout(user_info_layout)

        # --- 中间偏右：路径信息（垂直排列：上Run,下Home） ---
        path_container = QWidget()
        path_v_layout = QVBoxLayout(path_container)
        path_v_layout.setSpacing(2)  # 缩短上下行间距
        path_v_layout.setContentsMargins(0, 0, 0, 0)

        self.run_path_label = QLabel(self.format_path(self.vdj_run, "VDJ安装路径"))
        self.home_path_label = QLabel(self.format_path(self.vdj_home, "VDJ数据路径"))


        # 设置字体大小,防止文字过大显示不全
        path_style = "font-size: 10px; color: #555;"
        self.run_path_label.setStyleSheet(path_style)
        self.home_path_label.setStyleSheet(path_style)

        path_v_layout.addWidget(self.run_path_label)
        path_v_layout.addWidget(self.home_path_label)

        # --- 右侧：设置齿轮 ---
        btn_path_edit = QPushButton("⚙")
        btn_path_edit.setFixedSize(30, 30)  # 给齿轮固定大小
        # btn_path_edit.setStyleSheet("""
        #             QPushButton {
        #                 font-size: 18px;
        #                 border: none;
        #                 background: transparent;
        #                 color: #666;
        #             }
        #             QPushButton:hover { color: #000; }
        #         """)
        btn_path_edit.clicked.connect(self.edit_vdj_paths)

        # 将所有组件添加到水平布局
        user_info_layout.addWidget(self.avatar_label)
        user_info_layout.addWidget(self.user_detail_label)
        user_info_layout.addSpacing(100)  # 增加间距
        user_info_layout.addWidget(path_container)
        user_info_layout.addStretch()  # 关键：将右侧组件推到边缘
        user_info_layout.addWidget(btn_path_edit)

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
            # self.write_log("⚙ 配置已更新.若修改了端口,请停止并重新启动服务以生效.")

    def toggle_service(self):
        port = self.get_current_port()
        if self.api_process is None:
            exe_path = os.path.join(os.getcwd(), "app.exe")
            if not os.path.exists(exe_path):
                return self.write_log(f"❌ 找不到执行文件: {exe_path}")

            # 启动进程,重定向标准输出和错误流,并隐藏控制台窗口
            self.api_process = subprocess.Popen(
                [exe_path],
                env=dict(os.environ, PORT=port),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW
            )

            # 创建并启动专门读取 stdout 管道的线程
            self.log_thread = ProcessLogThread(self.api_process)
            self.log_thread.log_signal.connect(self.write_log)
            self.log_thread.start()

            self.service_start_time = time.time()
            self.btn_toggle_service.setText("停止服务")
            self.btn_toggle_service.setStyleSheet("background-color: #f44336; color: white;")
            self.write_log(f"🚀 服务已启动 (端口: {port})")
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
            self.write_log("📸 二维码已获取,请使用网易云音乐App扫码.")
        except:
            self.write_log("❌ 无法连接服务.")

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
                self.user_detail_label.setText(f"用户名: {p['nickname']}\n用户ID: {p['userId']}")
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
        if not self.current_cookie:
            return self.write_log("⚠ 请先登录")

        target_path = os.path.join(self.get_plugin_path(), "cookie.txt")
        try:
            with open(target_path, "w", encoding="utf-8") as f:
                f.write(self.current_cookie)
            self.write_log(f"🍪 Cookie 已保存: {target_path}")
        except Exception as e:
            self.write_log(f"❌ Cookie 保存失败: {str(e)}")

    def export_user_data(self):
        if not self.current_cookie:
            return self.write_log("⚠ 请先登录")

        port = self.get_current_port()
        try:
            url = f"http://localhost:{port}/login/status"
            res = requests.post(url, json={"cookie": self.current_cookie}, timeout=5).json()

            target_path = os.path.join(self.get_plugin_path(), "user_data.txt")
            with open(target_path, "w", encoding="utf-8") as f:
                json.dump(res, f, indent=4, ensure_ascii=False)

            self.write_log(f"🗎 UserData 已保存: {target_path}")
        except Exception as e:
            self.write_log(f"❌ UserData保存失败: {str(e)}")

    def show_tutorial(self):
        self.tutorial_win = TutorialDialog(self);
        self.tutorial_win.show()

    def update_runtime_ui(self, time_str):
        self.runtime_label.setText(f"已运行: {time_str}" if self.api_process else "已运行: 00:00:00")

    def write_log(self, text):


        try:
            # 1. 使用 ansi2html 转换颜色代码
            # 确保 self.ansi_converter = Ansi2HTMLConverter(inline=True) 已在 __init__ 初始化
            html_content = self.ansi_converter.convert(text, full=False)

            # 2. 格式化时间戳
            current_time = time.strftime("%H:%M:%S")
            # 组合成最终的 HTML 字符串,添加 <br> 确保换行
            log_entry = f'<span style="color:#888;">[{current_time}]</span> {html_content}<br>'

            # 3. 执行追加操作 (PySide6 正确流程)
            # 先移动光标到最后
            self.log_box.moveCursor(QTextCursor.End)
            # 插入 HTML 内容
            self.log_box.insertHtml(log_entry)
            # 再次移动光标确保视图跟随
            self.log_box.moveCursor(QTextCursor.End)

        except Exception as e:
            # 备用方案：如果转换失败,使用普通 append（会自动换行但无颜色）
            print(f"颜色转换失败: {e}")
            self.log_box.append(text)

    def load_ui_config(self):
        """
        解决不保存的关键：
        1. 优先检查本地 config.json。
        2. 只有当 JSON 缺失或路径为空时，才读取注册表。
        """
        self.vdj_home = ""
        self.vdj_run = ""
        self.is_first_run = True

        config_path = "config.json"

        # 1. 尝试从 JSON 读取记忆
        if os.path.exists(config_path):
            try:
                with open(config_path, "r", encoding="utf-8") as f:
                    d = json.load(f)
                    self.is_first_run = d.get('first_run', True)
                    self.vdj_home = d.get('vdj_home', "")
                    self.vdj_run = d.get('vdj_run', "")
            except:
                pass

        # 2. 如果 JSON 里没路径（首次运行或被删除），再查注册表
        if not self.vdj_home or not self.vdj_run:
            reg_home, reg_run = self.get_vdj_registry_paths()
            self.vdj_home = reg_home
            self.vdj_run = reg_run

            # 3. 如果从注册表读取成功，立即存入 JSON，防止下次丢失
            if self.vdj_home and self.vdj_run:
                self.save_config_to_json()

        if hasattr(self, 'run_path_label'):
            self.run_path_label.setText(self.format_path(self.vdj_run, "VDJ安装路径"))
        if hasattr(self, 'home_path_label'):
            self.home_path_label.setText(self.format_path(self.vdj_home, "VDJ数据路径"))

    def closeEvent(self, event):
        """修复你之前担心的覆盖问题"""
        try:
            # 不要直接写入 {"first_run": False}, 而是调用保存全量数据的函数
            self.save_config_to_json()

            if hasattr(self, 'health_thread'):
                self.health_thread.terminate()
            self.stop_service()
        except:
            pass
        event.accept()

    def get_vdj_registry_paths(self):
        try:
            key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\VirtualDJ")
            home = winreg.QueryValueEx(key, "HomeFolder")[0]
            run = winreg.QueryValueEx(key, "RunFolder64")[0]
            return home, run
        except:
            return "", ""

    def auto_deploy_plugin(self):
        """
        首次启动自动部署插件逻辑。
        检测目标目录是否存在 DLL，若无则自动放入并记录 LOG。
        """
        # 1. 定义源文件和目标路径
        plugin_name = "NeteaseCloudMusic.dll"
        source_path = os.path.join(os.getcwd(), plugin_name)

        # 假设 self.vdj_home 已经在实例中定义
        # 目标目录：Home/Plugins64/OnlineSources/
        target_dir = os.path.join(self.vdj_home, "Plugins64", "OnlineSources")
        target_path = os.path.join(target_dir, plugin_name)

        # 2. 判断是否需要部署
        if not os.path.exists(target_path):
            self.write_log("⚠️ 首次运行：插件未安装，正在尝试自动安装...")

            try:
                # 确保目标文件夹存在
                os.makedirs(target_dir, exist_ok=True)

                # 检查源文件是否存在
                if os.path.exists(source_path):
                    # 3. 执行拷贝
                    shutil.copy2(source_path, target_path)
                    self.write_log(f"✅ 自动安装成功：{plugin_name} 已放入插件目录")
                else:
                    self.write_log(f"❌ 自动安装失败：在运行目录未找到 {plugin_name}")

            except Exception as e:
                self.write_log(f"❌ 自动安装异常：{str(e)}")
        else:
            # 如果已经存在，可以输出一条静默日志或不输出
            self.write_log("插件已存在，跳过自动安装。")




if __name__ == "__main__":
    app = QApplication(sys.argv);
    window = MainWindow();
    window.show();
    sys.exit(app.exec())

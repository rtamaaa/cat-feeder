"""
============================================================
 SMART CAT FEEDER -- Android Application (Python + Kivy)
 UI dengan Bottom Navigation Bar + Left Drawer + Rich Iconography 🐾
============================================================
"""

import json
import os
import threading
from datetime import datetime
import requests

from kivy.app import App
from kivy.animation import Animation
from kivy.clock import Clock, mainthread
from kivy.core.window import Window
from kivy.core.text import LabelBase
from kivy.graphics import Color, RoundedRectangle, Rectangle, Line
from kivy.lang import Builder
from kivy.metrics import dp
from kivy.properties import (
    BooleanProperty, NumericProperty,
    ObjectProperty, StringProperty, ColorProperty
)
from kivy.uix.behaviors import ButtonBehavior
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.button import Button
from kivy.uix.floatlayout import FloatLayout
from kivy.uix.label import Label
from kivy.uix.screenmanager import Screen, ScreenManager, FadeTransition, SlideTransition
from kivy.uix.scrollview import ScrollView
from kivy.uix.slider import Slider
from kivy.uix.textinput import TextInput
from kivy.uix.widget import Widget

# Register Windows Segoe UI Emoji font so all emojis render on screen
if os.path.exists("C:/Windows/Fonts/seguiemj.ttf"):
    LabelBase.register(name="Roboto", fn_regular="C:/Windows/Fonts/seguiemj.ttf")
elif os.path.exists("C:/Windows/Fonts/seguisym.ttf"):
    LabelBase.register(name="Roboto", fn_regular="C:/Windows/Fonts/seguisym.ttf")

Window.size = (420, 780)
SESSION_FILE = "session.json"

KV = """
#:import hex kivy.utils.get_color_from_hex
#:import dp  kivy.metrics.dp

# ── REUSABLE COMPONENTS ──────────────────────────────────────

<GlassCard@BoxLayout>:
    orientation: 'vertical'
    padding: dp(16)
    spacing: dp(10)
    canvas.before:
        Color:
            rgba: hex('#0F1A2E')
        RoundedRectangle:
            pos: self.pos
            size: self.size
            radius: [dp(14)]
        Color:
            rgba: hex('#1E3A5F88')
        Line:
            rounded_rectangle: (self.x, self.y, self.width, self.height, dp(14))
            width: 1.2

<AccentCard@BoxLayout>:
    orientation: 'vertical'
    padding: dp(20)
    spacing: dp(10)
    canvas.before:
        Color:
            rgba: hex('#080E1C')
        RoundedRectangle:
            pos: self.pos
            size: self.size
            radius: [dp(16)]
        Color:
            rgba: hex('#3B82F622')
        RoundedRectangle:
            pos: self.pos
            size: self.size
            radius: [dp(16)]
        Color:
            rgba: hex('#3B82F6AA')
        Line:
            rounded_rectangle: (self.x, self.y, self.width, self.height, dp(16))
            width: 1.3

<PrimaryBtn@Button>:
    bold: True
    color: hex('#FFFFFF')
    background_normal: ''
    background_color: (0,0,0,0)
    size_hint_y: None
    height: dp(48)
    canvas.before:
        Color:
            rgba: hex('#2563EB') if self.state == 'down' else hex('#3B82F6')
        RoundedRectangle:
            pos: self.pos
            size: self.size
            radius: [dp(12)]

<GreenBtn@Button>:
    bold: True
    color: hex('#FFFFFF')
    background_normal: ''
    background_color: (0,0,0,0)
    size_hint_y: None
    height: dp(48)
    canvas.before:
        Color:
            rgba: hex('#059669') if self.state == 'down' else hex('#10B981')
        RoundedRectangle:
            pos: self.pos
            size: self.size
            radius: [dp(12)]

<ModernInput@TextInput>:
    font_size: '14sp'
    padding: [dp(14), dp(12), dp(14), dp(12)]
    size_hint_y: None
    height: dp(46)
    multiline: False
    background_color: hex('#060D1A')
    foreground_color: hex('#F1F5F9')
    cursor_color: hex('#38BDF8')
    hint_text_color: hex('#475569')
    canvas.after:
        Color:
            rgba: hex('#1E3A5F')
        Line:
            rounded_rectangle: (self.x, self.y, self.width, self.height, dp(8))
            width: 1

<SmallLabel@Label>:
    color: hex('#94A3B8')
    font_size: '11sp'
    bold: True
    size_hint_y: None
    height: dp(18)
    halign: 'left'
    text_size: self.size

<HDivider@Widget>:
    size_hint_y: None
    height: dp(1)
    canvas:
        Color:
            rgba: hex('#1E3A5F')
        Rectangle:
            pos: self.pos
            size: self.size

# ── APP ROOT ─────────────────────────────────────────────────

<RootLayout>:
    ScreenManager:
        id: sm
        size_hint: 1, 1

    DrawerDimmer:
        id: dimmer
        size_hint: None, None
        size: root.size
        pos: 0, 0
        opacity: 0

    LeftDrawer:
        id: drawer
        size_hint: None, None
        width: dp(270)
        height: root.height
        x: -dp(270)
        y: 0

# ── LEFT DRAWER ──────────────────────────────────────────────

<LeftDrawer>:
    orientation: 'vertical'
    canvas.before:
        Color:
            rgba: hex('#060D1A')
        Rectangle:
            pos: self.pos
            size: self.size
        Color:
            rgba: hex('#1E3A5FAA')
        Line:
            points: [self.right, self.y, self.right, self.top]
            width: 1.5

    # Drawer header
    BoxLayout:
        orientation: 'vertical'
        size_hint_y: None
        height: dp(130)
        padding: [dp(20), dp(16)]
        spacing: dp(4)
        canvas.before:
            Color:
                rgba: hex('#0D1826')
            Rectangle:
                pos: self.pos
                size: self.size

        BoxLayout:
            size_hint_y: None
            height: dp(44)
            spacing: dp(10)
            Label:
                text: '🐱 🐾'
                font_size: '30sp'
                size_hint_x: None
                width: dp(70)
            Label:
                text: 'Smart Feeder'
                font_size: '16sp'
                bold: True
                color: hex('#F1F5F9')
                halign: 'left'
                text_size: self.size

        Label:
            text: '✨ Pro IoT Feeder System'
            font_size: '11sp'
            color: hex('#38BDF8')
            size_hint_y: None
            height: dp(16)
            halign: 'left'
            text_size: self.size

        Label:
            text: '👤 User: ' + root.username_label
            font_size: '12sp'
            bold: True
            color: hex('#10B981')
            size_hint_y: None
            height: dp(18)
            halign: 'left'
            text_size: self.size

    # Menu items
    ScrollView:
        do_scroll_x: False
        BoxLayout:
            orientation: 'vertical'
            size_hint_y: None
            height: self.minimum_height
            padding: [dp(10), dp(14)]
            spacing: dp(6)

            DrawerItem:
                item_icon: '🏠'
                item_label: 'Beranda Utama'
                screen_name: 'pg_dash'
                drawer_ref: root

            DrawerItem:
                item_icon: '⏰'
                item_label: 'Jadwal Makan Otomatis'
                screen_name: 'pg_sched'
                drawer_ref: root

            DrawerItem:
                item_icon: '⚙️'
                item_label: 'Kalibrasi Servo & Delay'
                screen_name: 'pg_servo'
                drawer_ref: root

            DrawerItem:
                item_icon: '📜'
                item_label: 'Riwayat & Log Pakan'
                screen_name: 'pg_hist'
                drawer_ref: root

            DrawerItem:
                item_icon: '📡'
                item_label: 'Koneksi WiFi & Jaringan'
                screen_name: 'pg_wifi'
                drawer_ref: root

            HDivider:

            DrawerItem:
                item_icon: '🚪'
                item_label: 'Keluar (Logout)'
                screen_name: '__logout__'
                drawer_ref: root
                is_danger: True

    Widget:

# ── DRAWER ITEM ──────────────────────────────────────────────

<DrawerItem>:
    size_hint_y: None
    height: dp(48)
    spacing: dp(12)
    padding: [dp(14), dp(6)]
    canvas.before:
        Color:
            rgba: hex('#EF444422') if self.is_danger else (hex('#3B82F622') if self.is_active else (0,0,0,0))
        RoundedRectangle:
            pos: self.pos
            size: self.size
            radius: [dp(10)]
    on_release: self.do_navigate()

    Label:
        text: root.item_icon
        font_size: '20sp'
        size_hint_x: None
        width: dp(30)

    Label:
        text: root.item_label
        font_size: '13sp'
        bold: root.is_active
        color: hex('#EF4444') if root.is_danger else (hex('#60A5FA') if root.is_active else hex('#94A3B8'))
        halign: 'left'
        valign: 'middle'
        text_size: self.size

# ── DRAWER DIMMER ────────────────────────────────────────────

<DrawerDimmer>:
    canvas.before:
        Color:
            rgba: (0, 0, 0, 0.65 * self.opacity)
        Rectangle:
            pos: self.pos
            size: self.size

# ── TOP BAR ──────────────────────────────────────────────────

<TopBar@BoxLayout>:
    bar_title: ''
    root_ref: None
    size_hint_y: None
    height: dp(56)
    padding: [dp(6), dp(6), dp(14), dp(6)]
    spacing: dp(8)
    canvas.before:
        Color:
            rgba: hex('#080E1C')
        Rectangle:
            pos: self.pos
            size: self.size
        Color:
            rgba: hex('#1E3A5F')
        Line:
            points: [self.x, self.y, self.right, self.y]
            width: 1

    Button:
        text: '☰'
        font_size: '22sp'
        size_hint_x: None
        width: dp(46)
        color: hex('#60A5FA')
        background_normal: ''
        background_color: (0,0,0,0)
        on_release: root.root_ref.open_drawer() if root.root_ref else None
        canvas.before:
            Color:
                rgba: hex('#1E293B88') if self.state == 'down' else (0,0,0,0)
            RoundedRectangle:
                pos: self.pos
                size: self.size
                radius: [dp(8)]

    Label:
        text: root.bar_title
        font_size: '16sp'
        bold: True
        color: hex('#F1F5F9')
        halign: 'left'
        valign: 'middle'
        text_size: self.size

    Button:
        text: '🔄'
        font_size: '18sp'
        size_hint_x: None
        width: dp(42)
        color: hex('#38BDF8')
        background_normal: ''
        background_color: (0,0,0,0)
        on_release: root.root_ref.refresh_page() if root.root_ref else None
        canvas.before:
            Color:
                rgba: hex('#1E293B88') if self.state == 'down' else (0,0,0,0)
            RoundedRectangle:
                pos: self.pos
                size: self.size
                radius: [dp(8)]

# ── BOTTOM NAV BAR ───────────────────────────────────────────

<BottomNav@BoxLayout>:
    root_ref: None
    active_tab: 'pg_dash'
    size_hint_y: None
    height: dp(62)
    spacing: 0
    canvas.before:
        Color:
            rgba: hex('#060D1A')
        Rectangle:
            pos: self.pos
            size: self.size
        Color:
            rgba: hex('#1E3A5F')
        Line:
            points: [self.x, self.top, self.right, self.top]
            width: 1.2

    NavTab:
        tab_icon: '🏠'
        tab_label: 'Beranda'
        tab_target: 'pg_dash'
        nav_ref: root.root_ref

    NavTab:
        tab_icon: '⏰'
        tab_label: 'Jadwal'
        tab_target: 'pg_sched'
        nav_ref: root.root_ref

    NavTab:
        tab_icon: '⚙️'
        tab_label: 'Servo'
        tab_target: 'pg_servo'
        nav_ref: root.root_ref

    NavTab:
        tab_icon: '📜'
        tab_label: 'Riwayat'
        tab_target: 'pg_hist'
        nav_ref: root.root_ref

    NavTab:
        tab_icon: '📡'
        tab_label: 'WiFi'
        tab_target: 'pg_wifi'
        nav_ref: root.root_ref

# ── NAV TAB ──────────────────────────────────────────────────

<NavTab>:
    orientation: 'vertical'
    spacing: dp(2)
    padding: [0, dp(6)]
    canvas.before:
        Color:
            rgba: hex('#0F1A2E') if (root.nav_ref is not None and root.nav_ref.active_tab == root.tab_target) else (0,0,0,0)
        Rectangle:
            pos: self.pos
            size: self.size
    on_release: self.go()

    Label:
        text: root.tab_icon
        font_size: '22sp'
        size_hint_y: 0.58
        color: hex('#3B82F6') if (root.nav_ref is not None and root.nav_ref.active_tab == root.tab_target) else hex('#475569')

    Label:
        text: root.tab_label
        font_size: '9.5sp'
        bold: True if (root.nav_ref is not None and root.nav_ref.active_tab == root.tab_target) else False
        size_hint_y: 0.42
        color: hex('#60A5FA') if (root.nav_ref is not None and root.nav_ref.active_tab == root.tab_target) else hex('#64748B')

# ──────────────────────────────────────────────────────────────
# LOGIN SCREEN
# ──────────────────────────────────────────────────────────────

<LoginScreen>:
    name: 'login'
    canvas.before:
        Color:
            rgba: hex('#080E1C')
        Rectangle:
            pos: self.pos
            size: self.size

    BoxLayout:
        orientation: 'vertical'
        padding: [dp(24), 0]

        Widget:
            size_hint_y: None
            height: dp(30)

        # Hero Banner
        BoxLayout:
            orientation: 'vertical'
            size_hint_y: None
            height: dp(90)
            spacing: dp(3)
            Label:
                text: '🐱 🐟 🐾'
                font_size: '38sp'
                size_hint_y: None
                height: dp(46)
            Label:
                text: 'Smart Cat Feeder Pro'
                font_size: '22sp'
                bold: True
                color: hex('#F1F5F9')
                size_hint_y: None
                height: dp(26)
            Label:
                text: '✨ Otomatisasi Pakan Kucing Berbasis IoT'
                font_size: '11.5sp'
                color: hex('#38BDF8')
                size_hint_y: None
                height: dp(16)

        Widget:
            size_hint_y: None
            height: dp(14)

        # Mode Selector: Masuk vs Daftar
        BoxLayout:
            size_hint_y: None
            height: dp(42)
            spacing: dp(8)
            canvas.before:
                Color:
                    rgba: hex('#0F1A2E')
                RoundedRectangle:
                    pos: self.pos
                    size: self.size
                    radius: [dp(10)]

            Button:
                text: '🔑  Masuk'
                font_size: '13sp'
                bold: True
                color: hex('#FFFFFF') if not root.is_register_mode else hex('#64748B')
                background_normal: ''
                background_color: (0,0,0,0)
                on_release:
                    root.is_register_mode = False
                    root.msg = ""
                canvas.before:
                    Color:
                        rgba: hex('#2563EB') if not root.is_register_mode else (0,0,0,0)
                    RoundedRectangle:
                        pos: self.pos
                        size: self.size
                        radius: [dp(8)]

            Button:
                text: '📝  Daftar Akun Baru'
                font_size: '13sp'
                bold: True
                color: hex('#FFFFFF') if root.is_register_mode else hex('#64748B')
                background_normal: ''
                background_color: (0,0,0,0)
                on_release:
                    root.is_register_mode = True
                    root.msg = ""
                canvas.before:
                    Color:
                        rgba: hex('#059669') if root.is_register_mode else (0,0,0,0)
                    RoundedRectangle:
                        pos: self.pos
                        size: self.size
                        radius: [dp(8)]

        Widget:
            size_hint_y: None
            height: dp(10)

        # Form Card
        AccentCard:
            size_hint_y: None
            height: dp(370) if root.is_register_mode else dp(300)
            spacing: dp(6)
            padding: [dp(16), dp(14)]

            SmallLabel:
                text: '🌐  ALAMAT SERVER (URL)'
            ModernInput:
                id: inp_server
                text: root.server_url
                hint_text: 'http://catfeeder.tamamici.my.id'

            SmallLabel:
                text: '👤  USERNAME' + (' (MIN. 3 KARAKTER)' if root.is_register_mode else '')
            ModernInput:
                id: inp_user
                text: root.username
                hint_text: 'admin'

            SmallLabel:
                text: '🔑  PASSWORD' + (' (MIN. 6 KARAKTER)' if root.is_register_mode else '')
            ModernInput:
                id: inp_pass
                text: root.password
                password: True
                hint_text: '••••••••'

            BoxLayout:
                orientation: 'vertical'
                size_hint_y: None
                height: dp(60) if root.is_register_mode else 0
                opacity: 1 if root.is_register_mode else 0
                disabled: not root.is_register_mode
                spacing: dp(2)
                SmallLabel:
                    text: '📡  DEVICE ID FEEDER'
                ModernInput:
                    id: inp_dev
                    text: 'CAT_FEEDER_01'
                    hint_text: 'CAT_FEEDER_01'

            Widget:
                size_hint_y: None
                height: dp(4)

            Button:
                text: ('📝 ✨   DAFTAR AKUN BARU' if root.is_register_mode else '🚀 🐾   MASUK KE SISTEM')
                font_size: '13.5sp'
                bold: True
                color: hex('#FFFFFF')
                background_normal: ''
                background_color: (0,0,0,0)
                size_hint_y: None
                height: dp(46)
                on_release: (root.do_register() if root.is_register_mode else root.do_login())
                canvas.before:
                    Color:
                        rgba: hex('#059669') if root.is_register_mode else hex('#2563EB')
                    RoundedRectangle:
                        pos: self.pos
                        size: self.size
                        radius: [dp(10)]

        Label:
            text: root.msg
            color: hex('#38BDF8') if 'berhasil' in root.msg.lower() or 'selamat' in root.msg.lower() else hex('#EF4444')
            font_size: '11.5sp'
            size_hint_y: None
            height: dp(26)
            halign: 'center'
            text_size: self.size

        Widget:

# ──────────────────────────────────────────────────────────────
# MAIN SCREEN (holds bottom nav + pages)
# ──────────────────────────────────────────────────────────────

<MainScreen>:
    name: 'main'
    canvas.before:
        Color:
            rgba: hex('#080E1C')
        Rectangle:
            pos: self.pos
            size: self.size

    BoxLayout:
        orientation: 'vertical'

        # Inner screen manager for pages
        ScreenManager:
            id: inner_sm

        # Bottom Nav Bar
        BottomNav:
            id: bottom_nav
            root_ref: root
            active_tab: root.active_tab

# ──────────────────────────────────────────────────────────────
# DASHBOARD PAGE
# ──────────────────────────────────────────────────────────────

<PageDash>:
    name: 'pg_dash'
    canvas.before:
        Color:
            rgba: hex('#080E1C')
        Rectangle:
            pos: self.pos
            size: self.size

    BoxLayout:
        orientation: 'vertical'

        TopBar:
            bar_title: '🏠  Dashboard Feeder'
            root_ref: root.root_ref

        ScrollView:
            do_scroll_x: False
            BoxLayout:
                orientation: 'vertical'
                padding: [dp(14), dp(14)]
                spacing: dp(12)
                size_hint_y: None
                height: self.minimum_height

                # Status card
                GlassCard:
                    size_hint_y: None
                    height: dp(136)
                    padding: [dp(16), dp(12)]
                    spacing: dp(4)

                    BoxLayout:
                        size_hint_y: None
                        height: dp(32)
                        Label:
                            text: '📡  Status Perangkat'
                            bold: True
                            color: hex('#60A5FA')
                            font_size: '13sp'
                            halign: 'left'
                            text_size: self.size
                        Label:
                            text: root.status_badge
                            bold: True
                            color: root.status_color
                            font_size: '15sp'
                            halign: 'right'
                            text_size: self.size

                    HDivider:

                    BoxLayout:
                        size_hint_y: None
                        height: dp(28)
                        Label:
                            text: '⏱️  Terakhir Aktif:'
                            color: hex('#64748B')
                            font_size: '12sp'
                            halign: 'left'
                            text_size: self.size
                        Label:
                            text: root.last_seen
                            color: hex('#CBD5E1')
                            font_size: '12sp'
                            halign: 'right'
                            text_size: self.size

                    BoxLayout:
                        size_hint_y: None
                        height: dp(30)
                        Label:
                            text: '🕒  Jam DS3231 RTC:'
                            color: hex('#64748B')
                            font_size: '12sp'
                            halign: 'left'
                            text_size: self.size
                        Label:
                            text: root.rtc_time
                            bold: True
                            color: hex('#06B6D4')
                            font_size: '15sp'
                            halign: 'right'
                            text_size: self.size

                # Big Feed button
                Button:
                    size_hint_y: None
                    height: dp(76)
                    background_normal: ''
                    background_color: (0,0,0,0)
                    on_release: root.send_feed()
                    canvas.before:
                        Color:
                            rgba: hex('#92400E') if self.state == 'down' else hex('#0A1628')
                        RoundedRectangle:
                            pos: self.pos
                            size: self.size
                            radius: [dp(16)]
                        Color:
                            rgba: hex('#F59E0B44')
                        RoundedRectangle:
                            pos: self.pos
                            size: self.size
                            radius: [dp(16)]
                        Color:
                            rgba: hex('#F59E0B')
                        Line:
                            rounded_rectangle: (self.x, self.y, self.width, self.height, dp(16))
                            width: 1.8
                    Label:
                        text: '🍖 🐾   KASIH PAKAN SEKARANG   🐾 🍖'
                        font_size: '15sp'
                        bold: True
                        color: hex('#FEF3C7')
                        pos: self.parent.pos
                        size: self.parent.size

                Label:
                    text: root.feed_msg
                    color: hex('#38BDF8')
                    font_size: '12sp'
                    size_hint_y: None
                    height: dp(20)
                    halign: 'center'
                    text_size: self.size

                # Quick actions
                BoxLayout:
                    size_hint_y: None
                    height: dp(48)
                    spacing: dp(10)

                    Button:
                        text: '🕒 🔄  Sync Jam RTC'
                        font_size: '12sp'
                        bold: True
                        color: hex('#F1F5F9')
                        background_normal: ''
                        background_color: (0,0,0,0)
                        on_release: root.sync_rtc()
                        canvas.before:
                            Color:
                                rgba: hex('#4F46E5') if self.state == 'down' else hex('#6366F1')
                            RoundedRectangle:
                                pos: self.pos
                                size: self.size
                                radius: [dp(10)]

                    Button:
                        text: '📶 ⚙️  Setup Wi-Fi'
                        font_size: '12sp'
                        bold: True
                        color: hex('#F1F5F9')
                        background_normal: ''
                        background_color: (0,0,0,0)
                        on_release: root.go_wifi()
                        canvas.before:
                            Color:
                                rgba: hex('#0E7490') if self.state == 'down' else hex('#0891B2')
                            RoundedRectangle:
                                pos: self.pos
                                size: self.size
                                radius: [dp(10)]

                # Recent logs mini
                GlassCard:
                    size_hint_y: None
                    height: dp(150)
                    padding: [dp(14), dp(10)]
                    spacing: dp(8)

                    BoxLayout:
                        size_hint_y: None
                        height: dp(20)
                        Label:
                            text: '📋 🐾  Riwayat Pakan Terkini'
                            bold: True
                            color: hex('#F1F5F9')
                            font_size: '13sp'
                            halign: 'left'
                            text_size: self.size

                    HDivider:

                    ScrollView:
                        Label:
                            id: lbl_log
                            text: root.log_text
                            color: hex('#94A3B8')
                            font_size: '12sp'
                            size_hint_y: None
                            height: self.texture_size[1]
                            halign: 'left'
                            text_size: self.width, None

# ──────────────────────────────────────────────────────────────
# JADWAL PAGE
# ──────────────────────────────────────────────────────────────

<PageSched>:
    name: 'pg_sched'
    canvas.before:
        Color:
            rgba: hex('#080E1C')
        Rectangle:
            pos: self.pos
            size: self.size

    BoxLayout:
        orientation: 'vertical'

        TopBar:
            bar_title: '⏰  Jadwal Makan Otomatis'
            root_ref: root.root_ref

        ScrollView:
            do_scroll_x: False
            BoxLayout:
                orientation: 'vertical'
                padding: [dp(14), dp(14)]
                spacing: dp(12)
                size_hint_y: None
                height: self.minimum_height

                GlassCard:
                    size_hint_y: None
                    height: self.minimum_height
                    padding: [dp(14), dp(14)]
                    spacing: dp(10)

                    BoxLayout:
                        size_hint_y: None
                        height: dp(36)

                        Label:
                            text: '🥣 ⏰  Slot Jadwal Makan'
                            bold: True
                            color: hex('#F1F5F9')
                            font_size: '15sp'
                            halign: 'left'
                            text_size: self.size

                        Button:
                            text: '➕ 🥣  Tambah'
                            size_hint_x: None
                            width: dp(110)
                            font_size: '12sp'
                            bold: True
                            color: hex('#FFFFFF')
                            background_normal: ''
                            background_color: (0,0,0,0)
                            on_release: root.add_slot()
                            canvas.before:
                                Color:
                                    rgba: hex('#059669') if self.state == 'down' else hex('#10B981')
                                RoundedRectangle:
                                    pos: self.pos
                                    size: self.size
                                    radius: [dp(8)]

                    HDivider:

                    Label:
                        text: 'ℹ️ Maksimal 6 slot • Format Jam: HH & Menit: MM'
                        color: hex('#475569')
                        font_size: '11sp'
                        size_hint_y: None
                        height: dp(16)
                        halign: 'left'
                        text_size: self.size

                    BoxLayout:
                        id: box_slots
                        orientation: 'vertical'
                        spacing: dp(8)
                        size_hint_y: None
                        height: self.minimum_height

                    Widget:
                        size_hint_y: None
                        height: dp(6)

                    GreenBtn:
                        text: '💾 ✨   SIMPAN SEMUA JADWAL'
                        on_release: root.save_all()

                    Label:
                        text: root.msg
                        color: hex('#38BDF8')
                        font_size: '12sp'
                        size_hint_y: None
                        height: dp(20)
                        halign: 'center'
                        text_size: self.size

                # Tips card
                GlassCard:
                    size_hint_y: None
                    height: dp(80)
                    padding: [dp(14), dp(10)]
                    spacing: dp(4)
                    Label:
                        text: '💡 🐾  Informasi Jadwal'
                        bold: True
                        color: hex('#F59E0B')
                        font_size: '12.5sp'
                        size_hint_y: None
                        height: dp(18)
                        halign: 'left'
                        text_size: self.size
                    Label:
                        text: 'Jadwal tersimpan di chip LittleFS ESP8266 & dijalankan mandiri oleh RTC DS3231 walau tanpa WiFi.'
                        color: hex('#94A3B8')
                        font_size: '11sp'
                        size_hint_y: None
                        height: dp(32)
                        halign: 'left'
                        text_size: self.width, None

# ──────────────────────────────────────────────────────────────
# SERVO PAGE
# ──────────────────────────────────────────────────────────────

<PageServo>:
    name: 'pg_servo'
    canvas.before:
        Color:
            rgba: hex('#080E1C')
        Rectangle:
            pos: self.pos
            size: self.size

    BoxLayout:
        orientation: 'vertical'

        TopBar:
            bar_title: '⚙️  Kalibrasi Sudut Servo'
            root_ref: root.root_ref

        ScrollView:
            do_scroll_x: False
            BoxLayout:
                orientation: 'vertical'
                padding: [dp(14), dp(14)]
                spacing: dp(12)
                size_hint_y: None
                height: self.minimum_height

                GlassCard:
                    size_hint_y: None
                    height: dp(284)
                    padding: [dp(14), dp(14)]
                    spacing: dp(10)

                    Label:
                        text: '⚙️ 📐  Sudut Gerak & Delay Katup'
                        bold: True
                        color: hex('#F1F5F9')
                        font_size: '15sp'
                        size_hint_y: None
                        height: dp(22)
                        halign: 'left'
                        text_size: self.size

                    HDivider:

                    # Slider: Sudut Tutup
                    BoxLayout:
                        orientation: 'vertical'
                        size_hint_y: None
                        height: dp(56)
                        spacing: dp(2)
                        BoxLayout:
                            size_hint_y: None
                            height: dp(20)
                            Label:
                                text: '🔒  Sudut TUTUP (Katup Rapat):'
                                color: hex('#60A5FA')
                                font_size: '12sp'
                                bold: True
                                halign: 'left'
                                text_size: self.size
                            Label:
                                text: str(int(sl_close.value)) + '°'
                                color: hex('#F1F5F9')
                                font_size: '15sp'
                                bold: True
                                halign: 'right'
                                text_size: self.size
                        Slider:
                            id: sl_close
                            min: 0
                            max: 180
                            step: 1
                            value: root.close_angle

                    # Slider: Sudut Buka
                    BoxLayout:
                        orientation: 'vertical'
                        size_hint_y: None
                        height: dp(56)
                        spacing: dp(2)
                        BoxLayout:
                            size_hint_y: None
                            height: dp(20)
                            Label:
                                text: '🔓  Sudut BUKA (Pakan Keluar):'
                                color: hex('#10B981')
                                font_size: '12sp'
                                bold: True
                                halign: 'left'
                                text_size: self.size
                            Label:
                                text: str(int(sl_open.value)) + '°'
                                color: hex('#F1F5F9')
                                font_size: '15sp'
                                bold: True
                                halign: 'right'
                                text_size: self.size
                        Slider:
                            id: sl_open
                            min: 0
                            max: 180
                            step: 1
                            value: root.open_angle

                    # Slider: Durasi
                    BoxLayout:
                        orientation: 'vertical'
                        size_hint_y: None
                        height: dp(56)
                        spacing: dp(2)
                        BoxLayout:
                            size_hint_y: None
                            height: dp(20)
                            Label:
                                text: '⏳  Durasi Terbuka (Porsi):'
                                color: hex('#F59E0B')
                                font_size: '12sp'
                                bold: True
                                halign: 'left'
                                text_size: self.size
                            Label:
                                text: str(round(sl_dur.value/1000.0,1)) + ' detik'
                                color: hex('#F1F5F9')
                                font_size: '15sp'
                                bold: True
                                halign: 'right'
                                text_size: self.size
                        Slider:
                            id: sl_dur
                            min: 500
                            max: 8000
                            step: 100
                            value: root.duration_ms

                    PrimaryBtn:
                        text: '💾 ⚙️   SIMPAN SETTING SERVO'
                        on_release: root.save_settings(int(sl_close.value), int(sl_open.value), int(sl_dur.value))

                    Label:
                        text: root.msg
                        color: hex('#38BDF8')
                        font_size: '12sp'
                        size_hint_y: None
                        height: dp(20)
                        halign: 'center'
                        text_size: self.size

                # Riwayat servo
                GlassCard:
                    size_hint_y: None
                    height: dp(210)
                    padding: [dp(14), dp(12)]
                    spacing: dp(8)

                    BoxLayout:
                        size_hint_y: None
                        height: dp(30)

                        Label:
                            text: '📜 🔍  Riwayat Kalibrasi Servo'
                            bold: True
                            color: hex('#F1F5F9')
                            font_size: '13sp'
                            halign: 'left'
                            text_size: self.size

                        Button:
                            text: '🔄 🔃 Muat'
                            size_hint_x: None
                            width: dp(80)
                            font_size: '12sp'
                            bold: True
                            color: hex('#FFFFFF')
                            background_normal: ''
                            background_color: (0,0,0,0)
                            on_release: root.load_history()
                            canvas.before:
                                Color:
                                    rgba: hex('#334155') if self.state == 'down' else hex('#1E3A5F')
                                RoundedRectangle:
                                    pos: self.pos
                                    size: self.size
                                    radius: [dp(6)]

                    HDivider:

                    ScrollView:
                        Label:
                            text: root.hist_text
                            color: hex('#94A3B8')
                            font_size: '11sp'
                            size_hint_y: None
                            height: self.texture_size[1]
                            halign: 'left'
                            text_size: self.width, None

# ──────────────────────────────────────────────────────────────
# HISTORY PAGE
# ──────────────────────────────────────────────────────────────

<PageHist>:
    name: 'pg_hist'
    canvas.before:
        Color:
            rgba: hex('#080E1C')
        Rectangle:
            pos: self.pos
            size: self.size

    BoxLayout:
        orientation: 'vertical'

        TopBar:
            bar_title: '📜  Riwayat Log Pakan'
            root_ref: root.root_ref

        ScrollView:
            do_scroll_x: False
            BoxLayout:
                orientation: 'vertical'
                padding: [dp(14), dp(14)]
                spacing: dp(12)
                size_hint_y: None
                height: self.minimum_height

                GlassCard:
                    size_hint_y: None
                    height: self.minimum_height
                    padding: [dp(14), dp(14)]
                    spacing: dp(10)

                    BoxLayout:
                        size_hint_y: None
                        height: dp(30)

                        Label:
                            text: '📋 🐾  Log Pemberian Pakan'
                            bold: True
                            color: hex('#F1F5F9')
                            font_size: '15sp'
                            halign: 'left'
                            text_size: self.size

                        Button:
                            text: '🔄 🔃 Refresh'
                            size_hint_x: None
                            width: dp(94)
                            font_size: '12sp'
                            bold: True
                            color: hex('#FFFFFF')
                            background_normal: ''
                            background_color: (0,0,0,0)
                            on_release: root.load()
                            canvas.before:
                                Color:
                                    rgba: hex('#2563EB') if self.state == 'down' else hex('#3B82F6')
                                RoundedRectangle:
                                    pos: self.pos
                                    size: self.size
                                    radius: [dp(6)]

                    HDivider:

                    BoxLayout:
                        id: box_hist
                        orientation: 'vertical'
                        spacing: dp(8)
                        size_hint_y: None
                        height: self.minimum_height

# ──────────────────────────────────────────────────────────────
# WIFI PAGE
# ──────────────────────────────────────────────────────────────

<PageWifi>:
    name: 'pg_wifi'
    canvas.before:
        Color:
            rgba: hex('#080E1C')
        Rectangle:
            pos: self.pos
            size: self.size

    BoxLayout:
        orientation: 'vertical'

        TopBar:
            bar_title: '📡  Koneksi Wi-Fi ESP8266'
            root_ref: root.root_ref

        ScrollView:
            do_scroll_x: False
            BoxLayout:
                orientation: 'vertical'
                padding: [dp(14), dp(14)]
                spacing: dp(12)
                size_hint_y: None
                height: self.minimum_height

                GlassCard:
                    size_hint_y: None
                    height: dp(160)
                    padding: [dp(14), dp(14)]
                    spacing: dp(8)

                    Label:
                        text: '🌐 📡  Pilih Mode Konfigurasi'
                        bold: True
                        color: hex('#F1F5F9')
                        font_size: '14sp'
                        size_hint_y: None
                        height: dp(22)
                        halign: 'left'
                        text_size: self.size

                    HDivider:

                    BoxLayout:
                        size_hint_y: None
                        height: dp(54)
                        spacing: dp(10)

                        Button:
                            text: '📶  Mode AP Direct\\n(Hotspot CatFeeder)'
                            font_size: '11.5sp'
                            bold: True
                            color: hex('#FFFFFF')
                            background_normal: ''
                            background_color: (0,0,0,0)
                            halign: 'center'
                            on_release: root.set_mode(True)
                            canvas.before:
                                Color:
                                    rgba: hex('#059669') if root.is_ap else hex('#1E3A5F')
                                RoundedRectangle:
                                    pos: self.pos
                                    size: self.size
                                    radius: [dp(10)]

                        Button:
                            text: '🌐  Mode Remote Server\\n(Ganti WiFi via Web)'
                            font_size: '11.5sp'
                            bold: True
                            color: hex('#FFFFFF')
                            background_normal: ''
                            background_color: (0,0,0,0)
                            halign: 'center'
                            on_release: root.set_mode(False)
                            canvas.before:
                                Color:
                                    rgba: hex('#2563EB') if not root.is_ap else hex('#1E3A5F')
                                RoundedRectangle:
                                    pos: self.pos
                                    size: self.size
                                    radius: [dp(10)]

                    Label:
                        text: root.mode_note
                        color: hex('#F59E0B')
                        font_size: '11sp'
                        size_hint_y: None
                        height: dp(28)
                        halign: 'left'
                        text_size: self.size

                GlassCard:
                    size_hint_y: None
                    height: dp(206)
                    padding: [dp(14), dp(14)]
                    spacing: dp(8)

                    Label:
                        text: '📝 🔑  Kredensial Wi-Fi Baru'
                        bold: True
                        color: hex('#F1F5F9')
                        font_size: '14sp'
                        size_hint_y: None
                        height: dp(22)
                        halign: 'left'
                        text_size: self.size

                    HDivider:

                    SmallLabel:
                        text: '📡  NAMA WIFI (SSID)'
                    ModernInput:
                        id: inp_ssid
                        hint_text: 'Contoh: WiFi-Rumah-2.4G'

                    SmallLabel:
                        text: '🔒  PASSWORD WIFI'
                    ModernInput:
                        id: inp_pass
                        password: True
                        hint_text: 'Masukkan password WiFi'

                GreenBtn:
                    text: '🚀 📡   KIRIM KONFIGURASI KE ESP'
                    on_release: root.send()

                Label:
                    text: root.send_msg
                    color: hex('#38BDF8')
                    font_size: '12sp'
                    size_hint_y: None
                    height: dp(22)
                    halign: 'center'
                    text_size: self.size
"""

Builder.load_string(KV)


# ── SCHEDULE ROW ─────────────────────────────────────────────
class ScheduleRow(BoxLayout):
    enabled = BooleanProperty(True)
    slot_num = NumericProperty(1)

    def __init__(self, slot, enabled, hour, minute, on_delete, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'horizontal'
        self.size_hint_y = None
        self.height = dp(52)
        self.spacing = dp(8)
        self.padding = [dp(10), dp(6), dp(10), dp(6)]
        self.slot_num = slot
        self.enabled = enabled
        self._on_delete = on_delete

        with self.canvas.before:
            self._bg_c = Color(rgba=(0.08, 0.18, 0.3, 1) if enabled else (0.1, 0.13, 0.18, 1))
            self._bg_r = RoundedRectangle(pos=self.pos, size=self.size, radius=[dp(10)])
        self.bind(pos=self._upd, size=self._upd)

        self.btn = Button(
            text=f"⏰ J{slot}: {'AKTIF 🟢' if enabled else 'OFF ⚪'}",
            size_hint_x=0.36,
            background_normal='', background_color=(0, 0, 0, 0),
            font_size='11.5sp', bold=True, color=(1, 1, 1, 1)
        )
        with self.btn.canvas.before:
            self._btn_c = Color(rgba=(0.06, 0.72, 0.5, 1) if enabled else (0.27, 0.33, 0.42, 1))
            self._btn_r = RoundedRectangle(pos=self.btn.pos, size=self.btn.size, radius=[dp(8)])
        self.btn.bind(pos=self._upd_btn, size=self._upd_btn, on_release=self._toggle)
        self.add_widget(self.btn)

        self.in_h = TextInput(
            text=str(hour).zfill(2), size_hint_x=0.18, size_hint_y=None, height=dp(36),
            input_filter='int', multiline=False, font_size='15sp',
            background_color=(0.04, 0.08, 0.14, 1), foreground_color=(0.95, 0.97, 1, 1),
            halign='center', padding=[6, 8, 6, 8]
        )
        self.add_widget(self.in_h)
        self.add_widget(Label(text=':', size_hint_x=0.05, bold=True, color=(0.55, 0.65, 0.78, 1), font_size='18sp'))

        self.in_m = TextInput(
            text=str(minute).zfill(2), size_hint_x=0.18, size_hint_y=None, height=dp(36),
            input_filter='int', multiline=False, font_size='15sp',
            background_color=(0.04, 0.08, 0.14, 1), foreground_color=(0.95, 0.97, 1, 1),
            halign='center', padding=[6, 8, 6, 8]
        )
        self.add_widget(self.in_m)

        bdel = Button(text='🗑️', size_hint_x=0.15, background_normal='',
                      background_color=(0, 0, 0, 0), font_size='14sp', bold=True, color=(1, 1, 1, 1))
        with bdel.canvas.before:
            Color(rgba=(0.25, 0.08, 0.08, 1))
            RoundedRectangle(pos=bdel.pos, size=bdel.size, radius=[dp(8)])
        bdel.bind(on_release=lambda *a: self._on_delete(self))
        self.add_widget(bdel)

    def _upd(self, *a):
        self._bg_r.pos = self.pos
        self._bg_r.size = self.size

    def _upd_btn(self, *a):
        self._btn_r.pos = self.btn.pos
        self._btn_r.size = self.btn.size

    def _toggle(self, *a):
        self.enabled = not self.enabled
        self.btn.text = f"⏰ J{self.slot_num}: {'AKTIF 🟢' if self.enabled else 'OFF ⚪'}"
        self._btn_c.rgba = (0.06, 0.72, 0.5, 1) if self.enabled else (0.27, 0.33, 0.42, 1)
        self._bg_c.rgba = (0.08, 0.18, 0.3, 1) if self.enabled else (0.1, 0.13, 0.18, 1)

    def get_data(self):
        try:
            h, m = int(self.in_h.text or 0), int(self.in_m.text or 0)
        except ValueError:
            h, m = 0, 0
        return {'enabled': self.enabled, 'hour': max(0, min(23, h)), 'minute': max(0, min(59, m))}


# ── NAV TAB ──────────────────────────────────────────────────
class NavTab(ButtonBehavior, BoxLayout):
    tab_icon = StringProperty('')
    tab_label = StringProperty('')
    tab_target = StringProperty('')
    nav_ref = ObjectProperty(None, allownone=True)

    def go(self):
        if self.nav_ref:
            try:
                if hasattr(self.nav_ref, 'go_to'):
                    self.nav_ref.go_to(self.tab_target)
                elif hasattr(self.nav_ref, 'ids') and 'sm' in self.nav_ref.ids:
                    main = self.nav_ref.ids.sm.get_screen('main')
                    main.go_to(self.tab_target)
            except Exception as e:
                print(f"NavTab.go error: {e}")


# ── DRAWER ITEM ──────────────────────────────────────────────
class DrawerItem(ButtonBehavior, BoxLayout):
    item_icon = StringProperty('')
    item_label = StringProperty('')
    screen_name = StringProperty('')
    is_danger = BooleanProperty(False)
    is_active = BooleanProperty(False)
    drawer_ref = ObjectProperty(None, allownone=True)

    def do_navigate(self):
        if not self.drawer_ref:
            return
        if self.screen_name == '__logout__':
            self.drawer_ref.do_logout()
        else:
            self.drawer_ref.navigate(self.screen_name)


# ── DRAWER DIMMER ────────────────────────────────────────────
class DrawerDimmer(Widget):
    on_dimmer_tap = None

    def on_touch_down(self, touch):
        if self.opacity > 0.05 and self.collide_point(*touch.pos):
            return True
        return super().on_touch_down(touch)

    def on_touch_up(self, touch):
        if self.opacity > 0.05 and self.collide_point(*touch.pos):
            if self.on_dimmer_tap and callable(self.on_dimmer_tap):
                self.on_dimmer_tap()
            return True
        return super().on_touch_up(touch)


# ── LEFT DRAWER ──────────────────────────────────────────────
class LeftDrawer(BoxLayout):
    username_label = StringProperty("admin")
    _root = None

    def navigate(self, screen_name):
        if self._root:
            try:
                main = self._root.ids.sm.get_screen('main')
                main.go_to(screen_name)
            except Exception as e:
                print(f"Drawer.navigate error: {e}")
            self._root.close_drawer()

    def do_logout(self):
        if self._root:
            self._root.close_drawer()
            Clock.schedule_once(lambda dt: self._root.logout(), 0.25)


# ── ROOT LAYOUT ──────────────────────────────────────────────
class RootLayout(FloatLayout):
    _open = False

    def open_drawer(self):
        if self._open:
            return
        self._open = True
        dimmer = self.ids.dimmer
        drawer = self.ids.drawer
        dimmer.on_dimmer_tap = self.close_drawer
        Animation(opacity=1, duration=0.22, t='out_quad').start(dimmer)
        Animation(x=0, duration=0.22, t='out_quad').start(drawer)

    def close_drawer(self):
        if not self._open:
            return
        self._open = False
        dimmer = self.ids.dimmer
        drawer = self.ids.drawer
        dimmer.on_dimmer_tap = None
        Animation(opacity=0, duration=0.18).start(dimmer)
        Animation(x=-dp(270), duration=0.22, t='in_quad').start(drawer)

    def refresh_page(self):
        try:
            ms = self.ids.sm.get_screen('main')
            scr = ms.ids.inner_sm.current_screen
            if hasattr(scr, 'refresh'):
                scr.refresh()
        except Exception:
            pass

    def logout(self):
        sm = self.ids.sm
        sm.transition = SlideTransition(direction='right')
        sm.current = 'login'
        if os.path.exists(SESSION_FILE):
            os.remove(SESSION_FILE)


# ── MAIN SCREEN (host for bottom nav + pages) ────────────────
class MainScreen(Screen):
    active_tab = StringProperty('pg_dash')
    root_ref = ObjectProperty(None, allownone=True)

    # shared session
    server_url = ''
    token = ''
    username = ''

    def init(self, server_url, token, username, root_ref):
        self.server_url = server_url
        self.token = token
        self.username = username
        self.root_ref = root_ref

        inner = self.ids.inner_sm
        inner.transition = FadeTransition(duration=0.15)

        # inject ref into pages
        for name in ['pg_dash', 'pg_sched', 'pg_servo', 'pg_hist', 'pg_wifi']:
            p = inner.get_screen(name)
            p.server_url = server_url
            p.token = token
            p.username = username
            p.root_ref = root_ref

        # update drawer
        root_ref.ids.drawer.username_label = username
        root_ref.ids.drawer._root = root_ref

        self.go_to('pg_dash')
        Clock.schedule_once(lambda dt: inner.get_screen('pg_dash').refresh(), 0.2)

    def go_to(self, target):
        self.active_tab = target
        inner = self.ids.inner_sm
        inner.current = target
        drawer = self.root_ref.ids.drawer if self.root_ref else None
        if drawer:
            for child in drawer.walk():
                if isinstance(child, DrawerItem):
                    child.is_active = (child.screen_name == target)


# ── BASE PAGE ────────────────────────────────────────────────
class BasePage(Screen):
    root_ref = ObjectProperty(None, allownone=True)
    server_url = ''
    token = ''
    username = ''

    def on_enter(self):
        self.refresh()

    def refresh(self):
        pass

    def _get(self, path, cb, **params):
        def _thread():
            url = f"{self.server_url.rstrip('/')}{path}"
            qs = '&'.join(f"{k}={v}" for k, v in params.items())
            if qs:
                url += ('&' if '?' in url else '?') + qs
            headers = {'Authorization': f"Bearer {self.token}"}
            try:
                r = requests.get(url, headers=headers, timeout=5)
                cb(r.json())
            except Exception as e:
                cb(None)
        threading.Thread(target=_thread, daemon=True).start()

    def _post(self, path, payload, cb):
        def _thread():
            url = f"{self.server_url.rstrip('/')}{path}"
            headers = {'Authorization': f"Bearer {self.token}", 'Content-Type': 'application/json'}
            try:
                r = requests.post(url, json=payload, headers=headers, timeout=5)
                cb(r, r.json())
            except Exception as e:
                cb(None, None)
        threading.Thread(target=_thread, daemon=True).start()


# ── LOGIN SCREEN ─────────────────────────────────────────────
class LoginScreen(Screen):
    server_url = StringProperty("http://catfeeder.tamamici.my.id")
    username = StringProperty("admin")
    password = StringProperty("bukalah11")
    is_register_mode = BooleanProperty(False)
    msg = StringProperty("")

    def on_enter(self):
        if os.path.exists(SESSION_FILE):
            try:
                with open(SESSION_FILE) as f:
                    d = json.load(f)
                token = d.get("token")
                if token:
                    self.server_url = d.get("server_url", self.server_url)
                    self.username = d.get("username", self.username)
                    Clock.schedule_once(
                        lambda dt: self._do_init(self.server_url, token, self.username), 0.1)
            except Exception:
                pass

    def do_login(self):
        su = self.ids.inp_server.text.strip().rstrip('/')
        u = self.ids.inp_user.text.strip()
        p = self.ids.inp_pass.text.strip()
        if not (su and u and p):
            self.msg = "⚠️ Harap isi semua kolom!"
            return
        self.msg = "🔄 Menghubungkan ke server..."
        threading.Thread(target=self._login, args=(su, u, p), daemon=True).start()

    def _login(self, su, u, p):
        try:
            r = requests.post(f"{su}/api/auth.php?action=login",
                              json={'username': u, 'password': p}, timeout=6)
            d = r.json()
            if d.get('success'):
                tok = d['data']['token']
                with open(SESSION_FILE, 'w') as f:
                    json.dump({"server_url": su, "username": u, "token": tok}, f)
                self._ok(su, tok, u)
            else:
                self._err(d.get('message', 'Login gagal!'))
        except requests.exceptions.ConnectionError:
            self._err("❌ Tidak bisa terhubung ke server! Cek URL & WiFi.")
        except Exception as e:
            self._err(f"❌ {e}")

    def do_register(self):
        su = self.ids.inp_server.text.strip().rstrip('/')
        u = self.ids.inp_user.text.strip()
        p = self.ids.inp_pass.text.strip()
        dev = self.ids.inp_dev.text.strip() if hasattr(self.ids, 'inp_dev') else 'CAT_FEEDER_01'
        if not dev:
            dev = 'CAT_FEEDER_01'

        if not (su and u and p):
            self.msg = "⚠️ Harap isi server, username, dan password!"
            return
        if len(u) < 3:
            self.msg = "⚠️ Username minimal 3 karakter!"
            return
        if len(p) < 6:
            self.msg = "⚠️ Password minimal 6 karakter!"
            return

        self.msg = "🔄 Mendaftarkan akun baru..."
        threading.Thread(target=self._register, args=(su, u, p, dev), daemon=True).start()

    def _register(self, su, u, p, dev):
        try:
            r = requests.post(f"{su}/api/auth.php?action=register",
                              json={'username': u, 'password': p, 'device_id': dev}, timeout=6)
            d = r.json()
            if d.get('success'):
                tok = d['data']['token']
                with open(SESSION_FILE, 'w') as f:
                    json.dump({"server_url": su, "username": u, "token": tok}, f)
                self._ok(su, tok, u)
            else:
                self._err(d.get('message', 'Registrasi gagal!'))
        except requests.exceptions.ConnectionError:
            self._err("❌ Tidak bisa terhubung ke server! Cek URL & WiFi.")
        except Exception as e:
            self._err(f"❌ {e}")

    @mainthread
    def _ok(self, su, tok, u):
        self.msg = ""
        self._do_init(su, tok, u)

    @mainthread
    def _err(self, m):
        self.msg = m

    def _do_init(self, su, tok, u):
        root = self.manager.parent
        main = self.manager.get_screen('main')
        main.init(su, tok, u, root)
        self.manager.transition = SlideTransition(direction='left')
        self.manager.current = 'main'


# ── DASHBOARD PAGE ───────────────────────────────────────────
class PageDash(BasePage):
    status_badge = StringProperty("MEMERIKSA...")
    status_color = ColorProperty([0.22, 0.74, 0.97, 1])
    last_seen = StringProperty("-")
    rtc_time = StringProperty("-")
    feed_msg = StringProperty("")
    log_text = StringProperty("Memuat log...")
    _poll = None

    def on_enter(self):
        self.refresh()
        if not self._poll:
            self._poll = Clock.schedule_interval(lambda dt: self._fetch_status(), 5)

    def on_leave(self):
        if self._poll:
            self._poll.cancel()
            self._poll = None

    def refresh(self):
        if not self.token:
            return
        self._fetch_status()
        self._fetch_logs()

    def _fetch_status(self):
        self._get('/api/device_status.php?action=status', self._on_status)

    @mainthread
    def _on_status(self, data):
        if data and data.get('success'):
            d = data['data']
            s = d.get('status', 'offline')
            sec = d.get('seconds_ago')
            last_seen_val = d.get('last_seen')

            if s == 'online':
                self.status_badge = "🟢 ONLINE"
                self.status_color = [0.06, 0.72, 0.50, 1]
                self.last_seen = f"{sec} detik lalu" if sec is not None else "Online"
            elif s == 'warning':
                self.status_badge = "🟡 WARNING"
                self.status_color = [0.96, 0.62, 0.04, 1]
                self.last_seen = f"{sec} detik lalu" if sec is not None else "Beberapa saat lalu"
            else:
                self.status_badge = "🔴 OFFLINE"
                self.status_color = [0.94, 0.27, 0.27, 1]
                self.last_seen = str(last_seen_val) if last_seen_val else "Offline"

            rtc_val = d.get('rtc_time') or d.get('server_time') or '-'
            self.rtc_time = str(rtc_val)
        else:
            self.status_badge = "🔴 OFFLINE"
            self.status_color = [0.94, 0.27, 0.27, 1]
            self.last_seen = "Offline"
            self.rtc_time = "-"

    def _fetch_logs(self):
        self._get('/api/device_status.php?action=feeding-log&limit=8', self._on_logs)

    @mainthread
    def _on_logs(self, data):
        if data and data.get('success'):
            logs = data['data']['logs']
            if not logs:
                self.log_text = "Belum ada riwayat pemberian pakan."
                return
            lines = []
            for l in logs:
                t = "🍖 Manual Feed" if l['type'] == 'manual' else f"⏰ Jadwal Slot {l.get('schedule_slot', 1)}"
                s = "✅" if l['status'] == 'success' else "❌"
                lines.append(f"{s}  {l['executed_at']}  |  {t}")
            self.log_text = "\n".join(lines)

    def send_feed(self):
        self.feed_msg = "🔄 Mengirim perintah pakan..."
        self._post('/api/feed.php', {'device_id': 'CAT_FEEDER_01'}, self._on_feed)

    @mainthread
    def _on_feed(self, resp, data):
        if data and data.get('success'):
            self.feed_msg = "✅ Perintah FEED berhasil terkirim ke ESP8266!"
        elif resp and resp.status_code == 409:
            self.feed_msg = "⏳ Masih ada perintah pakan dalam antrean."
        else:
            self.feed_msg = f"⚠️ {data.get('message', 'Gagal') if data else 'Error koneksi'}"

    def sync_rtc(self):
        now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self.feed_msg = f"🔄 Sinkronisasi jam HP ({now}) ke RTC..."
        self._post('/api/rtc.php?action=set', {'datetime': now}, self._on_rtc)

    @mainthread
    def _on_rtc(self, resp, data):
        if data and data.get('success'):
            self.feed_msg = "✅ Waktu RTC DS3231 berhasil diselaraskan!"
        else:
            self.feed_msg = "⚠️ Gagal sinkronisasi RTC"

    def go_wifi(self):
        if self.root_ref:
            ms = self.root_ref.ids.sm.get_screen('main')
            ms.go_to('pg_wifi')


# ── JADWAL PAGE ──────────────────────────────────────────────
class PageSched(BasePage):
    msg = StringProperty("")
    _rows = []

    def refresh(self):
        if not self.token:
            return
        self._get('/api/schedule.php', self._on_data)

    @mainthread
    def _on_data(self, data):
        box = self.ids.box_slots
        box.clear_widgets()
        self._rows.clear()
        if data and data.get('success'):
            scheds = data['data']['schedules']
            slot = 1
            for k, s in scheds.items():
                row = ScheduleRow(slot=slot, enabled=s.get('enabled', True),
                                  hour=s.get('hour', 7), minute=s.get('minute', 0),
                                  on_delete=self._del_row)
                box.add_widget(row)
                self._rows.append(row)
                slot += 1
        if not self._rows:
            self.add_slot()

    def add_slot(self):
        if len(self._rows) >= 6:
            self.msg = "⚠️ Maksimal 6 jadwal otomatis."
            return
        n = len(self._rows) + 1
        row = ScheduleRow(slot=n, enabled=True, hour=8, minute=0, on_delete=self._del_row)
        self.ids.box_slots.add_widget(row)
        self._rows.append(row)

    def _del_row(self, row):
        self.ids.box_slots.remove_widget(row)
        self._rows.remove(row)
        for i, r in enumerate(self._rows):
            r.slot_num = i + 1
            r.btn.text = f"⏰ J{r.slot_num}: {'AKTIF 🟢' if r.enabled else 'OFF ⚪'}"

    def save_all(self):
        payload = [r.get_data() for r in self._rows]
        self.msg = "🔄 Menyimpan jadwal ke server & ESP..."
        self._post('/api/schedule.php', {'schedules': payload}, self._on_save)

    @mainthread
    def _on_save(self, resp, data):
        if data and data.get('success'):
            n = data.get('data', {}).get('total_saved', len(self._rows))
            self.msg = f"✅ {n} Jadwal tersimpan & terkirim ke ESP8266!"
        else:
            self.msg = f"⚠️ {data.get('message', 'Gagal') if data else 'Error'}"


# ── SERVO PAGE ───────────────────────────────────────────────
class PageServo(BasePage):
    close_angle = NumericProperty(0)
    open_angle = NumericProperty(90)
    duration_ms = NumericProperty(2000)
    msg = StringProperty("")
    hist_text = StringProperty("Tekan '🔄 🔃 Muat' untuk melihat riwayat.")

    def refresh(self):
        if not self.token:
            return
        self._get('/api/servo_settings.php?action=get', self._on_data)

    @mainthread
    def _on_data(self, data):
        if data and data.get('success'):
            s = data['data']['settings']
            self.close_angle = s.get('close_angle', 0)
            self.open_angle = s.get('open_angle', 90)
            self.duration_ms = s.get('duration_ms', 2000)

    def save_settings(self, ca, oa, dur):
        self.msg = "🔄 Menyimpan pengaturan servo..."
        self._post('/api/servo_settings.php?action=set',
                   {'close_angle': ca, 'open_angle': oa, 'duration_ms': dur}, self._on_save)

    @mainthread
    def _on_save(self, resp, data):
        if data and data.get('success'):
            self.msg = "✅ Pengaturan servo tersimpan & dikirim ke ESP8266!"
        else:
            self.msg = f"⚠️ {data.get('message', 'Gagal') if data else 'Error'}"

    def load_history(self):
        self.hist_text = "🔄 Memuat log riwayat..."
        self._get('/api/servo_settings.php?action=logs&limit=15', self._on_hist)

    @mainthread
    def _on_hist(self, data):
        if not data:
            self.hist_text = "❌ Gagal memuat riwayat"
            return
        logs = data.get('data', {}).get('logs', [])
        if not logs:
            self.hist_text = "Belum ada riwayat perubahan servo."
            return
        lines = []
        for l in logs:
            d = round(l['duration_ms'] / 1000.0, 1)
            lines.append(f"🕒 {l['created_at']}\n   🔒 Tutup: {l['close_angle']}°  |  🔓 Buka: {l['open_angle']}°  |  ⏳ Delay: {d}s\n   👤 Diubah oleh: {l['changed_by']}\n")
        self.hist_text = "\n".join(lines)


# ── HISTORY PAGE ─────────────────────────────────────────────
class PageHist(BasePage):

    def refresh(self):
        self.load()

    def load(self):
        if not self.token:
            return
        self._get('/api/device_status.php?action=feeding-log&limit=30', self._on_data)

    @mainthread
    def _on_data(self, data):
        box = self.ids.box_hist
        box.clear_widgets()
        if not data or not data.get('success'):
            box.add_widget(Label(text="❌ Gagal memuat log riwayat.", color=(0.93, 0.27, 0.27, 1),
                                 size_hint_y=None, height=dp(40)))
            return
        logs = data['data']['logs']
        if not logs:
            box.add_widget(Label(text="Belum ada riwayat pemberian pakan.", color=(0.58, 0.64, 0.74, 1),
                                 size_hint_y=None, height=dp(40), halign='center'))
            return

        for l in logs:
            ok = l['status'] == 'success'
            tipe = "🍖 Manual Feed" if l['type'] == 'manual' else f"⏰ Jadwal Makan Slot {l.get('schedule_slot', 1)}"
            status_text = "✅ Berhasil" if ok else "❌ Gagal"
            
            row = BoxLayout(orientation='horizontal', size_hint_y=None, height=dp(58),
                            spacing=dp(10), padding=[dp(14), dp(8)])
            with row.canvas.before:
                _c = Color(rgba=(0.05, 0.14, 0.26, 1) if ok else (0.18, 0.04, 0.04, 1))
                _r = RoundedRectangle(pos=row.pos, size=row.size, radius=[dp(10)])

            def _bind(r2, rr):
                def _p(w, v): rr.pos = v
                def _s(w, v): rr.size = v
                r2.bind(pos=_p, size=_s)
            _bind(row, _r)

            row.add_widget(Label(text="🐾" if ok else "⚠️", font_size='22sp',
                                 size_hint_x=None, width=dp(34)))
            col = BoxLayout(orientation='vertical', spacing=dp(2))
            
            # Row title + status
            top_line = BoxLayout(orientation='horizontal')
            l1 = Label(text=tipe, bold=True, color=(0.94, 0.97, 1, 1), font_size='13sp',
                       halign='left', text_size=(None, None))
            l1.bind(size=lambda w, v: setattr(w, 'text_size', (v[0], None)))
            l_st = Label(text=status_text, bold=True,
                         color=(0.06, 0.72, 0.5, 1) if ok else (0.93, 0.27, 0.27, 1),
                         font_size='11.5sp', halign='right', size_hint_x=None, width=dp(80))
            top_line.add_widget(l1)
            top_line.add_widget(l_st)
            
            l2 = Label(text="🕒 " + l['executed_at'], color=(0.58, 0.64, 0.74, 1), font_size='11sp',
                       halign='left', text_size=(None, None))
            l2.bind(size=lambda w, v: setattr(w, 'text_size', (v[0], None)))
            
            col.add_widget(top_line)
            col.add_widget(l2)
            row.add_widget(col)
            box.add_widget(row)


# ── WIFI PAGE ────────────────────────────────────────────────
class PageWifi(BasePage):
    is_ap = BooleanProperty(True)
    mode_note = StringProperty("ℹ️ Hubungkan HP ke Wi-Fi 'CatFeeder-Setup' (192.168.4.1) dulu!")
    send_msg = StringProperty("")

    def refresh(self):
        pass

    def set_mode(self, ap):
        self.is_ap = ap
        self.mode_note = ("ℹ️ Hubungkan HP ke Wi-Fi 'CatFeeder-Setup' (192.168.4.1) dulu!"
                          if ap else "🌐 Server akan kirim perintah SET_WIFI ke ESP saat online.")

    def send(self):
        ssid = self.ids.inp_ssid.text.strip()
        pwd = self.ids.inp_pass.text.strip()
        if not ssid:
            self.send_msg = "⚠️ SSID Wi-Fi tidak boleh kosong!"
            return
        self.send_msg = "🔄 Mengirim konfigurasi Wi-Fi..."
        if self.is_ap:
            threading.Thread(target=self._ap, args=(ssid, pwd), daemon=True).start()
        else:
            self._post('/api/wifi_set.php',
                       {'device_id': 'CAT_FEEDER_01', 'ssid': ssid, 'password': pwd},
                       self._on_srv)

    def _ap(self, ssid, pwd):
        try:
            requests.post("http://192.168.4.1/wifi", data={'ssid': ssid, 'password': pwd}, timeout=6)
            self._smsg("✅ Konfigurasi Wi-Fi terkirim ke AP! ESP akan restart.")
        except Exception:
            self._smsg("❌ Gagal hubungi 192.168.4.1 (Pastikan konek ke CatFeeder-Setup)")

    @mainthread
    def _on_srv(self, resp, data):
        if data and data.get('success'):
            self.send_msg = "✅ Perintah pergantian Wi-Fi tersimpan di server!"
        else:
            self.send_msg = f"⚠️ {data.get('message', 'Gagal') if data else 'Error'}"

    @mainthread
    def _smsg(self, m):
        self.send_msg = m


# ── APP ──────────────────────────────────────────────────────
class SmartCatFeederApp(App):
    def build(self):
        self.title = "Smart Cat Feeder Pro 🐾"
        root = RootLayout()
        sm = root.ids.sm

        # Outer screens
        sm.add_widget(LoginScreen(name='login'))

        # MainScreen with inner SM
        main = MainScreen(name='main')
        inner = main.ids.inner_sm
        inner.add_widget(PageDash(name='pg_dash'))
        inner.add_widget(PageSched(name='pg_sched'))
        inner.add_widget(PageServo(name='pg_servo'))
        inner.add_widget(PageHist(name='pg_hist'))
        inner.add_widget(PageWifi(name='pg_wifi'))
        sm.add_widget(main)

        return root


if __name__ == '__main__':
    SmartCatFeederApp().run()

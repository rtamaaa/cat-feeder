# -*- mode: python ; coding: utf-8 -*-
import os
from kivy_deps import sdl2, glew, angle
from PyInstaller.building.build_main import Tree

block_cipher = None

datas = []
if os.path.exists('session.json'):
    datas.append(('session.json', '.'))

excluded_packages = [
    'tkinter', 'kivy.garden',
    'torch', 'torchvision', 'torchaudio',
    'scipy', 'numpy', 'pandas', 'polars', 'matplotlib', 'seaborn',
    'cv2', 'opencv_python', 'opencv_contrib_python', 'mediapipe', 'ultralytics',
    'django', 'flask', 'fastapi', 'starlette', 'uvicorn',
    'PyQt5', 'PySide6', 'shiboken6', 'pyqtgraph',
    'pygments', 'IPython', 'jupyter', 'tensorboard',
    'google', 'azure', 'botocore', 'boto3',
    'docx', 'fpdf2', 'openpyxl', 'label_studio', 'labelImg'
]

a = Analysis(
    ['app/frontend/main.py'],
    pathex=[os.path.abspath('.')],
    binaries=[],
    datas=datas,
    hiddenimports=[
        'kivy',
        'kivy.core.window.window_sdl2',
        'kivy.core.text.text_sdl2',
        'kivy.core.image.img_sdl2',
        'kivy.core.clipboard.clipboard_winctypes',
        'kivy.graphics.cgl_backend.cgl_glew',
        'kivy.graphics.cgl_backend.cgl_gl',
        'kivy.graphics.cgl_backend.cgl_mock',
        'requests',
        'urllib3',
        'charset_normalizer',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=excluded_packages,
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name='SmartCatFeederPro',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)

extra_trees = []
for p in (sdl2.dep_bins + glew.dep_bins + angle.dep_bins):
    if os.path.exists(p):
        extra_trees.append(Tree(p))

coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    *extra_trees,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='SmartCatFeederPro',
)

# 📱 Panduan Build File .APK (Android) Smart Cat Feeder

File `.apk` Android dari aplikasi Python/Kivy harus dikompilasi menggunakan tool **Buildozer (Android SDK + NDK + Cython)** yang membutuhkan sistem operasi **Linux / Cloud Builder**.

Karena Anda menggunakan **Windows**, berikut **2 cara paling mudah dan gratis** untuk menghasilkan file `.apk`:

---

## 🚀 Cara 1: Menggunakan GitHub Actions (Otomatis & Paling Cepat)

Kami telah membuatkan file otomasi:
👉 **[`.github/workflows/build-apk.yml`](file:///c:/Users/LENOVO/Desktop/Pakan-kucing/.github/workflows/build-apk.yml)**

### Langkah-langkah:
1. Upload/Push folder project ini ke akun GitHub Anda (Private atau Public).
2. Di halaman repository GitHub Anda, klik tab **Actions**.
3. Pilih workflow **"Build Android APK (Buildozer)"** di sebelah kiri.
4. Klik tombol **Run workflow** > pilih branch `main` > klik **Run workflow**.
5. Server Ubuntu di cloud GitHub akan otomatis meng-compile `.apk` (memakan waktu ~10-15 menit untuk build pertama).
6. Setelah selesai (centang hijau), scroll ke bagian paling bawah (**Artifacts**) dan download file:
   📦 **`SmartCatFeeder-Android-APK`** (berisi file `.apk` siap install di HP Android).

---

## 🌐 Cara 2: Menggunakan Google Colab (Gratis via Browser)

Jika belum menghubungkan ke GitHub, Anda bisa meng-compile `.apk` langsung lewat Google Chrome:

1. Buka [Google Colab](https://colab.research.google.com/) > klik **New Notebook**.
2. Ubah Runtime: Menu **Runtime** > **Change runtime type** > pilih CPU/GPU standard.
3. Buat cell baru dan jalankan kode setup ini:
   ```bash
   # 1. Install dependensi buildozer & fix compatibility Python 3.13 (cgi module)
   !sudo apt update
   !sudo apt install -y git zip unzip openjdk-17-jdk autoconf libtool pkg-config zlib1g-dev libncurses5-dev libncursesw5-dev libtinfo5 cmake libffi-dev libssl-dev
   !pip install --upgrade "cython<3.0" "buildozer>=1.5.0" "kivy>=2.3.0" legacy-cgi standard-imghdr
   ```
4. Upload file `main.py` dan `buildozer.spec` ke folder files di Google Colab.
5. Jalankan perintah kompilasi:
   ```bash
   !buildozer -v android debug
   ```
6. Setelah proses selesai, file `.apk` akan muncul di folder `bin/` pada panel kiri Google Colab, klik kanan > **Download**.

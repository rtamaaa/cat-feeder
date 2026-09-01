# 🍏 Panduan Build & Packaging Smart Cat Feeder ke iOS (.ipa / Xcode)

Aplikasi **Smart Cat Feeder Pro** dikembangkan menggunakan framework **Kivy (Python)**. Untuk mengompilasi aplikasi Kivy ke platform iOS (iPhone/iPad), digunakan toolchain resmi **`kivy-ios`**.

---

## 📌 Ketentuan Resmi Apple untuk iOS
> [!IMPORTANT]
> Apple mewajibkan pembuatan paket aplikasi iOS (`.ipa` / Xcode project) dilakukan di lingkungan **macOS** dengan **Xcode Command Line Tools** dan akun **Apple Developer** (untuk signing / sideload ke iPhone).

Ada **2 Metode** yang bisa Anda pilih:
1. **Metode A (Otomatis & Tanpa Mac Fisik):** Menggunakan **GitHub Actions** (Cloud macOS Runner Gratis).
2. **Metode B (Lokal di Mac):** Menggunakan terminal macOS dengan Xcode.

---

## 🚀 Metode A: Build Otomatis via GitHub Actions (Rekomendasi)

Kami sudah menyiapkan file otomasi workflow:
👉 **[`.github/workflows/build-ios.yml`](file:///c:/Users/LENOVO/Desktop/Pakan-kucing/.github/workflows/build-ios.yml)**

### Langkah-langkah:
1. Push repository project ini ke akun GitHub Anda (Private / Public repository).
2. Buka tab **Actions** di repository GitHub Anda.
3. Pilih workflow **"Build iOS App (kivy-ios)"** > klik **Run workflow**.
4. GitHub Actions (server macOS Cloud) akan otomatis:
   - Menginstall toolchain `kivy-ios` & dependensi (Python 3, SDL2, requests, dsb).
   - Mengompilasi source code Python frontend menjadi project **Xcode iOS Native**.
5. Setelah build selesai (~15-20 menit), download file **`SmartCatFeeder-iOS-Project.zip`** pada bagian **Artifacts**.
6. Project Xcode tersebut bisa langsung dibuka di Xcode untuk di-deploy ke iPhone via kabel Lightning/USB-C atau di-export menjadi `.ipa`.

---

## 💻 Metode B: Build Manual di Komputer Mac (macOS)

Jika Anda memiliki komputer Mac (MacBook / Mac Mini / iMac):

### 1. Prasyarat:
- Install **Xcode** dari Mac App Store.
- Buka terminal dan pastikan Xcode Command Line Tools aktif:
  ```bash
  xcode-select --install
  ```
- Install Homebrew & library pendukung:
  ```bash
  brew install autoconf automake libtool pkg-config
  ```

### 2. Install kivy-ios Toolchain:
```bash
python3 -m venv venv-ios
source venv-ios/bin/activate
pip install --upgrade pip
pip install cython kivy-ios
```

### 3. Kompilasi Recipes:
```bash
toolchain build python3 kivy requests urllib3 certifi charset_normalizer idna
```

### 4. Generate Xcode Project:
```bash
toolchain create SmartCatFeeder app/frontend
```

### 5. Buka & Jalankan di Xcode:
1. Masuk ke folder yang baru dibuat:
   ```bash
   open smartcatfeeder-ios/SmartCatFeeder.xcodeproj
   ```
2. Di Xcode:
   - Pilih tab **Signing & Capabilities** > pilih **Personal Team** (Akun Apple ID gratis Anda).
   - Sambungkan iPhone Anda ke Mac via kabel USB.
   - Pilih target device: **iPhone Anda** (atau Simulator iOS).
   - Klik tombol **Play (▶ Run)** untuk meng-install dan menjalankan aplikasi langsung di iPhone.

---

## 📱 Sideloading ke iPhone (Tanpa Mac)
Jika Anda memiliki file `.ipa`:
- Gunakan tools sideloading seperti **AltStore**, **Sideloadly**, atau **TrollStore** di PC Windows untuk meng-install file `.ipa` ke iPhone dengan Apple ID Anda.

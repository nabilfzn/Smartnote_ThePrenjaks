# 📕 SmartNote

**SmartNote** adalah solusi pembelajaran cerdas yang memanfaatkan teknologi **IoT** dan **Generative AI** untuk meningkatkan efektivitas belajar. Dengan kemampuan untuk merekam dan merangkum penjelasan audio secara otomatis, SmartNote membantu memastikan setiap materi tetap terdokumentasi meski disampaikan secara lisan.

Sistem ini secara instan mengubah rekaman menjadi **ringkasan** dan **modul pembelajaran** yang jelas dan terstruktur. Selain itu, SmartNote juga menyediakan **Quiz Generator** yang menghasilkan soal-soal berdasarkan materi tersebut, sehingga siswa dapat langsung mengukur pemahaman mereka.

> SmartNote, Ringkas Mengajar Cerdas Mencatat

---

## 🚀 Fitur Utama

- ✅ User **Signup & Login**
- 🎤 Rekam suara dengan perangkat SmartNote
- 📝 Transkripsi otomatis audio menjadi teks
- ✏️ Edit hasil transkripsi
- 📚 Buat rangkuman atau modul dari transkripsi
- 🧠 Buat dan edit quiz berbasis AI
- 👩‍🏫 Guru membagikan quiz ke siswa melalui ID
- 👨‍🎓 Siswa mengerjakan quiz dan mendapat feedback serta rekomendasi belajar

## 🚀 Cara Menggunakan SmartNote
---


### 1. Pastikan Versi Python
Gunakan **Python 3.10** atau versi yang sesuai dengan dependensi dalam proyek ini.

### 2. Buat Virtual Environment
Untuk menjaga lingkungan tetap bersih dan terisolasi:
```bash
python -m venv venv
```
### 3. Aktifkan Virtual Environment
  - Windows:
  ```bash
  venv\Scripts\activate
  ```
  - MacOS/Linux:
  ```bash
  source venv/bin/activate
  ```
### 4. Install Semua Library
Pastikan sudah berada di direktori utama proyek. Jalankan:

```bash
pip install -r requirements.txt
```

### 5. Buat File .env
Buat file .env di root project (satu folder dengan app.py dan flask.py) dan isi dengan variabel berikut:

```bash
GOOGLE_API_KEY=your_openai_api_key
```
Contoh:

```bash
GOOGLE_API_KEY=sk-xxxxxxxxxxxxxxxx
```
#### ⚠️ Catatan Penting:
- Pastikan semua perangkat (komputer, ESP32, dan server Flask) berada dalam satu jaringan WiFi yang sama.
- URL yang tidak sesuai (salah IP atau port) akan menyebabkan koneksi gagal antara Streamlit dan Flask.

<br>
<br>

## 🧠 Langkah Menjalankan Aplikasi
1. Pada folder _app/_init_.py_ ganti nama databasemu:
```bash
app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///nama_database_kamu.db'
```
2. generate database dengan Flask:
```bash
python init_db.py
```
3. Jalankan app server Flask:
```bash
python app.py
```


<br>
<br>


## 🕹️ Alur Menggunakan Aplikasi
## 🧭 Alur Penggunaan

### 1. **Autentikasi Pengguna**
- Pengguna membuat akun melalui halaman **Signup**
- Masuk menggunakan akun yang sudah terdaftar (**Login**)

---

### 2. **Rekam & Transkripsi Audio**
- Masuk ke halaman **Upload Audio**
- Rekam suara langsung melalui perangkat SmartNote
- Aplikasi akan mentranskripsi audio secara otomatis
- Hasil transkrip bisa dilihat di halaman **List Audio** (Sidebar)

---

### 3. **Edit Transkrip**
- Buka halaman **Transkrip** dari sidebar
- Pilih salah satu transkrip untuk **diedit** jika ada kesalahan

---

### 4. **Buat Modul/Rangkuman**
- Dari halaman transkrip, pilih transkrip yang akan dibuat modul
- Klik tombol **Buat Modul**
- Modul yang sudah dibuat akan muncul di halaman **List Modul**

---

### 5. **Buat & Edit Quiz**
- Dari modul, klik **Buat Quiz**
- Quiz akan digenerate oleh AI dari isi modul
- Edit soal/opsi quiz jika dibutuhkan pada halaman **Quiz**

---

### 6. **Akses Guru**
- Guru bisa melihat kunci jawaban quiz
- ID quiz akan terlihat di URL, misalnya: `/quiz/42`
- Guru membagikan ID tersebut ke siswa

---

### 7. **Akses Siswa**
- Siswa klik menu **Kerjakan Quiz**
- Masukkan ID quiz yang dibagikan guru (misalnya `42`)
- Siswa menjawab soal
- Setelah selesai, siswa akan mendapat:
  - Feedback benar/salah
  - Rekomendasi belajar dari AI

---

## ⚙️ Teknologi yang Digunakan

- Python + Flask
- SQLAlchemy (ORM)
- Jinja2 (Template)
- HTML/CSS/JS
- GEMINI API (untuk AI transcribe, modul, rangkuman, feedback, generate quiz)
---

```bash
## 📂 Struktur Proyek 

SMARTNOTE_FINAL/
├── app.py
├── init_db.py
├── requirements.txt
├── README.md
├── .env
├── instance/
│   └── smartnote.db
├── app/
│   ├── __init__.py
│   ├── models.py
│   ├── routes/
│   │   ├── auth.py
│   │   ├── conn.py
│   │   ├── modul.py
│   │   ├── quiz.py
│   │   ├── summarize.py
│   │   ├── transkrip.py
│   │   └── upload.py
│   ├── static/
│   │   └── suara_guru.mp3
│   └── templates/
│       └── components/
│           ├── sidebar.html
│           ├── arsip-rekaman.html
│           └── arsip-transkrip.html
```



from flask import Flask, render_template, request, jsonify, Blueprint, session, redirect, flash, url_for, current_app
import google.generativeai as genai
import json
import math
import PyPDF2
import os
from ..models import Modul
from .. import db
from collections import Counter
from werkzeug.utils import secure_filename
from ..models import db, Quiz, Question, Choice, StudentAnswer
import re
from dotenv import load_dotenv



# Blueprint untuk quiz
quizz = Blueprint('quizz', __name__)

# Konfigurasi API Gemini
load_dotenv()  
API_KEY = os.getenv("GEMINI_API_KEY")
genai.configure(api_key=API_KEY)



@quizz.route('/quiz')
def index():
    return render_template('form_quis.html')

@quizz.route('/kerjakan')
def kerjakan_quiz():
    return render_template('kerjakan_quiz.html')

def extract_text_from_pdf(file_path):
    with open(file_path, 'rb') as file:
        pdf_reader = PyPDF2.PdfReader(file)
        text = ""
        for page in pdf_reader.pages:
            text += page.extract_text() + "\n"
    return text

def extract_text_from_file(file_path):
    ext = os.path.splitext(file_path)[1].lower()
    if ext == '.pdf':
        return extract_text_from_pdf(file_path)
    elif ext in ['.txt', '.md', '.html']:
        try:
            with open(file_path, 'r', encoding='utf-8') as file:
                return file.read()
        except:
            with open(file_path, 'r', encoding='latin-1') as file:
                return file.read()
    else:
        return "Format file tidak didukung"

def generate_questions(modul_content, jumlah_soal):
    model_soal = genai.GenerativeModel(
        'models/gemini-2.0-flash',
        generation_config=genai.types.GenerationConfig(
            temperature=0.2,
            top_p=0.1,
            top_k=40,
            max_output_tokens=4096
        )
    )

    # Distribusi Bloom
    c1_count = math.ceil(jumlah_soal * 0.20)
    c2_count = math.ceil(jumlah_soal * 0.25)
    c3_count = math.ceil(jumlah_soal * 0.25)
    c4_count = math.ceil(jumlah_soal * 0.15)
    c5_count = math.ceil(jumlah_soal * 0.10)
    c6_count = math.floor(jumlah_soal * 0.05)

    prompt_soal = f"""
Gunakan isi modul pembelajaran berikut sebagai satu-satunya sumber informasi untuk membuat soal.

=== MULAI MODUL ===
{modul_content}
=== AKHIR MODUL ===

Buatkan {jumlah_soal} soal pilihan ganda berdasarkan modul pembelajaran di atas.

Instruksi penting:
1. Setiap soal harus memiliki SATU kategori taksonomi Bloom (C1-C6) yang tepat:
    - C1: Mengingat
    - C2: Memahami
    - C3: Menerapkan
    - C4: Menganalisis
    - C5: Mengevaluasi
    - C6: Mencipta
    
2. Distribusikan soal berdasarkan taksonomi Bloom:
    - C1: {c1_count} soal
    - C2: {c2_count} soal
    - C3: {c3_count} soal
    - C4: {c4_count} soal
    - C5: {c5_count} soal
    - C6: {c6_count} soal

3. Setiap soal HARUS memiliki 5 pilihan jawaban (A-E) dengan SATU jawaban benar.
4. Sertakan penjelasan untuk setiap soal.
5. Format hasil HARUS dalam JSON:
{{
  "soal": [
    {{
      "pertanyaan": "...",
      "kategori_taksonomi": "C1",
      "pilihan": [
        "A. ...",
        "B. ...",
        "C. ...",
        "D. ...",
        "E. ..."
      ],
      "jawaban_benar": "A",
      "penjelasan": "..."
    }}
  ]
}}
"""

    try:
        response = model_soal.generate_content(
            prompt_soal,
            request_options={"timeout": 600}
        )
        response_text = response.text.strip()
        start_idx = response_text.find('{')
        end_idx = response_text.rfind('}')
        if start_idx != -1 and end_idx != -1:
            json_text = response_text[start_idx:end_idx+1]
            hasil_soal = json.loads(json_text)
            return hasil_soal
        else:
            return {"error": "Format JSON tidak valid dalam respons"}
    except json.JSONDecodeError as je:
        return {"error": f"Error parsing JSON: {str(je)}", "raw_text": response_text}
    except Exception as e:
        return {"error": f"Error saat menghasilkan soal: {str(e)}"}

@quizz.route('/generate/<int:modul_id>', methods=['GET', 'POST'])
def generate(modul_id):
    modul = Modul.query.get_or_404(modul_id)

    if request.method == 'GET':
        # Ambil data modul
        data_modul = modul.data_modul or ''
        
        # Cek jika content adalah HTML
        if data_modul.strip().lower().startswith(('<!doctype', '<html')):
            # Konversi ke teks biasa (tanpa BeautifulSoup)
            import re
            # Hapus semua tag HTML
            isi_modul = re.sub(r'<[^>]+>', ' ', data_modul)
            # Hapus whitespace berlebih
            isi_modul = re.sub(r'\s+', ' ', isi_modul).strip()
            # Pisahkan paragraf dengan baris baru ganda
            isi_modul = re.sub(r'\.(?=\s)', '.\n\n', isi_modul)
        else:
            # Coba parse sebagai JSON
            try:
                modul_data = json.loads(data_modul)
                # Jika berhasil, proses JSON ke teks
                if isinstance(modul_data, dict):
                    isi_modul = ""
                    
                    # Ekstrak judul dan deskripsi
                    if 'judul_modul' in modul_data:
                        isi_modul += f"# {modul_data['judul_modul']}\n\n"
                    
                    if 'deskripsi_singkat' in modul_data:
                        isi_modul += f"{modul_data['deskripsi_singkat']}\n\n"
                    
                    # Proses struktur bab jika ada
                    if 'Bab' in modul_data and isinstance(modul_data['Bab'], list):
                        for bab in modul_data['Bab']:
                            if isinstance(bab, dict):
                                if 'Sub_Bab' in bab:
                                    isi_modul += f"## {bab['Sub_Bab']}\n\n"
                                
                                if 'isi' in bab:
                                    isi_bersih = bab['isi'].replace('<br>', '\n')
                                    isi_modul += f"{isi_bersih}\n\n"
                    
                    # Tambahkan rangkuman
                    if 'rangkuman' in modul_data:
                        isi_modul += f"## Rangkuman\n\n{modul_data['rangkuman']}\n\n"
                    
                    # Jika tidak ada ekstraksi, gunakan fallback
                    if not isi_modul.strip():
                        if 'isi_modul' in modul_data:
                            isi_modul = modul_data['isi_modul']
                        else:
                            isi_modul = "\n\n".join([str(value) for key, value in modul_data.items() 
                                                if not isinstance(value, (dict, list))])
                else:
                    isi_modul = str(modul_data)
            except Exception as e:
                # Jika bukan JSON valid, gunakan data mentah
                isi_modul = data_modul
                print(f"Error memproses data: {str(e)}")

        # Tampilkan form generate dengan modul yang dipilih
        return render_template('form_quis.html', modul=modul, isi_modul=isi_modul)

    # Jika POST, baru proses generate
    jumlah_soal = int(request.form.get('jumlahSoal', 20))
    modul_content = request.form.get('modulText')  # Ambil dari textarea

    # Cek apakah request adalah AJAX (XHR)
    is_ajax = request.headers.get('X-Requested-With') == 'XMLHttpRequest'

    if not modul_content or not modul_content.strip():
        if is_ajax:
            return jsonify({'error': 'Teks modul kosong. Silakan isi terlebih dahulu.'})
        else:
            flash('Teks modul kosong. Silakan isi terlebih dahulu.', 'danger')
            return redirect(url_for('quizz.generate', modul_id=modul_id))

    # Coba untuk mendapatkan judul
    try:
        if modul_content.startswith('#'):
            # Jika teks dimulai dengan heading
            modul_title = modul_content.split('\n')[0].strip('# ')
        else:
            # Gunakan judul dari model modul
            modul_title = modul.title or 'Tanpa Judul'
    except:
        modul_title = 'Modul'

    try:
        hasil_soal = generate_questions(modul_content, jumlah_soal)

        if "soal" not in hasil_soal:
            if is_ajax:
                return jsonify({'error': f"Gagal generate soal: {hasil_soal.get('error', '')}"})
            else:
                flash(f"Gagal generate soal: {hasil_soal.get('error', '')}", 'danger')
                return redirect(url_for('quizz.generate', modul_id=modul_id))
        
        # Jika berhasil generate soal, simpan ke database
        with current_app.app_context():
            quiz = Quiz(
                title=f"Quiz dari: {modul_title}",
                modul_id=modul_id
            )
            db.session.add(quiz)
            db.session.flush()
            quiz_id = quiz.id

            for item in hasil_soal['soal']:
                q = Question(
                    quiz_id=quiz_id,
                    pertanyaan=item['pertanyaan'],
                    kategori_taksonomi=item['kategori_taksonomi'],
                    jawaban_benar=item['jawaban_benar'],
                    penjelasan=item['penjelasan']
                )
                db.session.add(q)
                db.session.flush()

                for opt in item['pilihan']:
                    label, text = opt.split('.', 1)
                    db.session.add(Choice(
                        question_id=q.id,
                        label=label.strip(),
                        text=text.strip()
                    ))
            db.session.commit()
        
        # Modifikasi untuk menampilkan hasil di quiz.html
        if is_ajax:
            return jsonify({
                'success': True,
                'quiz_id': quiz_id,
                'redirect_url': url_for('quizz.view_quiz', quiz_id=quiz_id)
            })
        else:
            return redirect(url_for('quizz.view_quiz', quiz_id=quiz_id))
            
    except Exception as e:
        # Error handling
        if is_ajax:
            return jsonify({'error': f"Terjadi kesalahan: {str(e)}"})
        else:
            flash(f"Terjadi kesalahan: {str(e)}", 'danger')
            return redirect(url_for('quizz.generate', modul_id=modul_id))

@quizz.route('/quiz/<int:quiz_id>/view', methods=['GET'])
def view_quiz(quiz_id):
    quiz = Quiz.query.get_or_404(quiz_id)
    questions = Question.query.filter_by(quiz_id=quiz_id).all()
    
    # Format data untuk template quiz.html yang baru 
    soal_data = []
    for q in questions:
        choices = Choice.query.filter_by(question_id=q.id).all()
        soal_data.append({
            "id": q.id,
            "pertanyaan": q.pertanyaan,
            "kategori_taksonomi": q.kategori_taksonomi,
            "jawaban_benar": q.jawaban_benar,
            "penjelasan": q.penjelasan,
            "pilihan": choices
        })

    # Render template quiz.html yang sudah dimodifikasi
    return render_template('quiz.html', quiz=quiz, soal_data=soal_data)


@quizz.route('/quiz/<int:quiz_id>')
def student_quiz(quiz_id):
    quiz = Quiz.query.get_or_404(quiz_id)
    questions = Question.query.filter_by(quiz_id=quiz.id).all()
    soal_data = []
    for q in questions:
        choices = Choice.query.filter_by(question_id=q.id).all()
        soal_data.append({
            "id": q.id,
            "pertanyaan": q.pertanyaan,
            "kategori": q.kategori_taksonomi,
            "pilihan": choices
        })
    return render_template('student_quiz.html', quiz=quiz, soal_data=soal_data)


@quizz.route('/submit_answers/<int:quiz_id>', methods=['POST'])
def submit_answers(quiz_id):
    quiz = Quiz.query.get_or_404(quiz_id)
    answers = request.form
    feedback = []
    student_answers = []
    soal_data = []

    for question_id, answer in answers.items():
        question = Question.query.get(int(question_id.split('_')[1]))
        correct_answer = question.jawaban_benar
        is_correct = (answer == correct_answer)

        soal_data.append({
            'id': question.id,
            'pertanyaan': question.pertanyaan,
            'jawaban_siswa': answer,
            'jawaban_benar': correct_answer,
            'kategori_taksonomi': question.kategori_taksonomi,
            'pilihan': [{'label': c.label, 'text': c.text} for c in question.choices],
            'is_correct': is_correct,
            'penjelasan': question.penjelasan
        })

        student_answer = StudentAnswer(
            student_id="siswa123",
            question_id=question.id,
            answer=answer,
            correct=is_correct
        )
        student_answers.append(student_answer)

    db.session.add_all(student_answers)
    db.session.commit()

    salah_soal = [soal for soal in soal_data if not soal['is_correct']]
    jumlah_salah = len(salah_soal)
    kategori_counter = Counter([soal['kategori_taksonomi'] for soal in salah_soal])

    rekomendasi = rekomendasi_belajar(jumlah_salah, kategori_counter)

    return render_template(
        'feedback.html',
        feedback=feedback,
        soal_data=soal_data,
        rekomendasi=rekomendasi,
        title=quiz.title,
        total_salah=jumlah_salah,
        kategori_salah_counter=kategori_counter
    )

def format_bold(text):
    return re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", text)

def rekomendasi_belajar(jumlah_salah, kategori_counter):
    if jumlah_salah == 0:
        return {
            "analisis": format_bold("Selamat! Kamu tidak memiliki kelemahan yang signifikan."),
            "rekomendasi": [format_bold("Kamu sudah menguasai materi dengan sangat baik!")],
            "tips_efektif": [format_bold("Pertahankan kebiasaan belajarmu yang sudah baik.")],
            "saran_penutup": format_bold("Terus pertahankan prestasimu dan bantu teman-temanmu belajar juga.")
        }

    kategori_rinci = "\n".join([f"- {kategori}: {jumlah} soal salah" for kategori, jumlah in kategori_counter.items()])
    kategori_daftar = ", ".join(kategori_counter.keys())

    prompt = f"""
Saya siswa yang baru saja menyelesaikan kuis dengan total 20 soal.

Hasil saya:
- {jumlah_salah} soal salah dari 20 soal
- Rincian kesalahan berdasarkan kategori taksonomi Bloom:
{kategori_rinci}

Berdasarkan data tersebut, jelaskan:
1. Dimana kelemahan saya secara umum dan spesifik,
2. Kategori taksonomi yang paling perlu saya perhatikan,
3. Berikan rekomendasi belajar sesuai taksonomi Bloom.

Format output harus JSON dengan struktur:
{{
  "analisis": "...",
  "rekomendasi": ["1. ...", "2. ..."],
  "tips_efektif": ["..."],
  "saran_penutup": "..."
}}

Gunakan bahasa Indonesia yang jelas, singkat, dan ramah untuk siswa.
"""

    try:
        model = genai.GenerativeModel(
            'models/gemini-2.0-flash',
            generation_config=genai.types.GenerationConfig(
                temperature=0.7,
                top_p=0.1,
                top_k=40,
                max_output_tokens=4096
            )
        )
        response = model.generate_content(prompt)
        response_text = response.text.strip()

        print("Gemini response raw:\n", response_text)

        if response_text.startswith("```json"):
            response_text = re.sub(r"^```json", "", response_text)
            response_text = re.sub(r"```$", "", response_text).strip()

        data = json.loads(response_text)

        # Terapkan format bold
        data["analisis"] = format_bold(data["analisis"])
        data["saran_penutup"] = format_bold(data["saran_penutup"])
        data["rekomendasi"] = [format_bold(r) for r in data["rekomendasi"]]
        data["tips_efektif"] = [format_bold(t) for t in data["tips_efektif"]]

        return data

    except Exception as e:
        print(f"Rekomendasi fallback: {e}")
        return {
            "analisis": format_bold("Kamu masih perlu memperkuat beberapa aspek dalam pembelajaran, terutama pada kategori yang paling banyak salah."),
            "rekomendasi": [
                format_bold("1. Pelajari ulang konsep dasar dari kategori yang sering salah."),
                format_bold("2. Gunakan video pembelajaran untuk memperjelas pemahaman."),
                format_bold("3. Buat catatan ringkas dan peta konsep."),
                format_bold("4. Latih soal-soal sejenis secara bertahap."),
                format_bold("5. Diskusikan dengan teman atau guru.")
            ],
            "tips_efektif": [
                format_bold("Gunakan teknik belajar aktif seperti menjelaskan ulang."),
                format_bold("Atur waktu belajar dan buat target mingguan."),
                format_bold("Belajar di lingkungan yang tenang dan fokus.")
            ],
            "saran_penutup": format_bold("Kesalahan adalah bagian dari proses belajar. Tetap semangat dan terus berkembang!")
        }

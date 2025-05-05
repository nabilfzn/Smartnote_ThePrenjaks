from flask import Blueprint, request, redirect, url_for, flash, render_template, session
from ..models import User, Transkrip, Summarize, Modul
from .. import db
import google.generativeai as genai
import json
import os
from dotenv import load_dotenv
load_dotenv()

# Konfigurasi API
GOOGLE_API_KEY = os.getenv("GEMINI_API_KEY")
genai.configure(api_key=GOOGLE_API_KEY)

# Inisialisasi blueprint
summarize_bp = Blueprint('summarize', __name__)

def clean_json_output(text):
    try:
        teks = text.replace('```json', '').replace('```', '')
        teks = teks.replace('~~~json', '').replace('~~~', '')
        return teks.strip()
    except Exception as e:
        flash(f"Error saat membersihkan output: {str(e)}", 'error')
        return ""


@summarize_bp.route('/generate-summarize/<int:transkrip_id>', methods=['GET', 'POST'])
def generate_summarize(transkrip_id):
    user_id = session.get('user_id')
    if not user_id:
        flash("Silakan login terlebih dahulu", "warning")
        return redirect(url_for('auth.login'))

    transkrip = Transkrip.query.filter_by(id=transkrip_id, user_id=user_id).first_or_404()

    if not transkrip.teks:
        flash('Teks transkrip kosong', 'error')
        return redirect(url_for('upload.view_transkrip', transkrip_id=transkrip_id))

    model_summarize = genai.GenerativeModel('models/gemini-2.0-flash')

    prompt_summarize = """
    Tolong buatkan rangkuman dari materi berikut ini. Output HARUS berbentuk JSON murni seperti contoh di bawah dan SEMUA field wajib diisi, walaupun materinya tidak lengkap:
    [
        {
            "judul_rangkuman": "Judul",
            "pengertian_utama": "Pengertian ringkas",
            "poin_rangkuman": ["Poin 1", "Poin 2", "Poin 3"],
            "contoh_ilustrasi": ["Contoh 1", "Contoh 2"],
            "kesimpulan": "Kesimpulan akhir"
        }
    ]
    """

    try:
        response = model_summarize.generate_content(
            [prompt_summarize, transkrip.teks],
            request_options={"timeout": 600}
        )

        teks_summarize = clean_json_output(response.text.strip())

        try:
            summary_json = json.loads(teks_summarize)
            summary_text = json.dumps(summary_json, ensure_ascii=False, indent=2)

            new_summary = Summarize(
                data_summary=summary_text,
                transkrip_id=transkrip_id,
                user_id=user_id
            )
            db.session.add(new_summary)
            db.session.commit()

            flash('Summarize berhasil dibuat', 'success')
            return redirect(url_for('summarize.show_summary', summarize_id=new_summary.id))

        except json.JSONDecodeError:
            flash('Gagal parsing JSON dari output Gemini', 'error')
            return redirect(url_for('upload.view_transkrip', transkrip_id=transkrip_id))

    except Exception as e:
        flash(f'Error: {str(e)}', 'error')
        return redirect(url_for('upload.view_transkrip', transkrip_id=transkrip_id))


@summarize_bp.route('/show_summary/<int:summarize_id>')
def show_summary(summarize_id):
    user_id = session.get('user_id')
    if not user_id:
        flash("Silakan login terlebih dahulu.", "warning")
        return redirect(url_for('auth.login'))

    summary_obj = Summarize.query.filter_by(id=summarize_id, user_id=user_id).first_or_404()
    transkrip = Transkrip.query.get(summary_obj.transkrip_id)

    try:
        summary_data = json.loads(summary_obj.data_summary)[0]  # Ambil objek pertama dari array
    except (json.JSONDecodeError, IndexError, TypeError):
        flash("Data rangkuman rusak atau tidak valid.", "danger")
        summary_data = {
            "judul_rangkuman": "Rangkuman Tidak Valid",
            "pengertian_utama": "",
            "poin_rangkuman": [],
            "contoh_ilustrasi": [],
            "kesimpulan": ""
        }

    return render_template(
        'show_summary.html',
        summary_obj=summary_obj,   # objek dari database
        summary_data=summary_data, # hasil parsing JSON
        transkrip=transkrip
    )



@summarize_bp.route('/summary')
def list_summary():
    user_id = session.get('user_id')
    if not user_id:
        return redirect(url_for('auth.login'))

    all_summary = Summarize.query.filter_by(user_id=user_id).order_by(Summarize.created_at.desc()).all()

    for s in all_summary:
        try:
            parsed = json.loads(s.data_summary)
            s.judul_rangkuman = parsed[0].get('judul_rangkuman', None)
            s.pengertian_utama = parsed[0].get('pengertian_utama', None)
        except Exception:
            s.judul_rangkuman = None
            s.pengertian_utama = None

    return render_template('list_summary.html', summary_list=all_summary)

    




from flask import Blueprint, render_template, request, redirect, url_for, session, flash, current_app, jsonify
from werkzeug.utils import secure_filename
import os
import google.generativeai as genai
from ..models import db, Transkrip
from datetime import datetime
from bs4 import BeautifulSoup


upload = Blueprint('upload', __name__)
GOOGLE_API_KEY = 'AIzaSyDBV4t5y7oNh05oZnQxTYcK3rA1FiBd1Wc'
genai.configure(api_key=GOOGLE_API_KEY)


@upload.route('/upload', methods=['GET', 'POST'])
def upload_audio():
    if request.method == 'POST':
        audios = request.files.getlist('audio')
        user_id = session.get('user_id')

        if not user_id:
            flash('Anda harus login', 'error')
            return redirect(url_for('auth.login'))

        if audios:
            all_transcribe = []
            model = genai.GenerativeModel('models/gemini-2.0-flash-lite')

            upload_folder = os.path.join(current_app.root_path, 'static')
            os.makedirs(upload_folder, exist_ok=True)

            for audio in audios:
                if audio.filename == '':
                    continue

                filename = secure_filename(audio.filename)
                path = os.path.join(upload_folder, filename)
                audio.save(path)

                mime_type = 'audio/mpeg'

                with open(path, "rb") as f:
                    gemini_file = genai.upload_file(f, mime_type=mime_type)

                response = model.generate_content([
                    "Tolong transkripkan file audio ini ke dalam bentuk teks percakapan.",
                    gemini_file
                ])
                teks = response.text
                all_transcribe.append(teks)

            combined = "\n\n".join(all_transcribe)
            new_transkrip = Transkrip(name=audios[0].filename, teks=combined, user_id=user_id)
            db.session.add(new_transkrip)
            db.session.commit()

            return redirect(url_for('upload.daftar_soal', transkrip_id=new_transkrip.id))

    return render_template('upload.html')


@upload.route('/reprocess_audio', methods=['POST'])
def reprocess_audio():
    if 'user_id' not in session:
        flash('Anda harus login', 'error')
        return redirect(url_for('auth.login'))

    filename = request.form.get('filename')
    if not filename:
        flash('Nama file tidak valid', 'error')
        return redirect(request.referrer)

    # Path ke file audio
    upload_folder = os.path.join(current_app.root_path, 'static')
    path = os.path.join(upload_folder, filename)

    if not os.path.exists(path):
        flash('File audio tidak ditemukan', 'error')
        return redirect(request.referrer)

    # Proses transkripsi ulang
    model = genai.GenerativeModel('models/gemini-2.0-flash-lite')
    mime_type = 'audio/mpeg'

    with open(path, "rb") as f:
        gemini_file = genai.upload_file(f, mime_type=mime_type)

    response = model.generate_content([
        "Tolong transkripkan file audio ini ke dalam bentuk teks percakapan.",
        gemini_file
    ])
    teks = response.text

    # Update transkrip di database
    transkrip = Transkrip.query.filter_by(name=filename).first()
    if transkrip:
        transkrip.teks = teks
        transkrip.updated_at = datetime.now()
    else:
        transkrip = Transkrip(name=filename, teks=teks, user_id=session['user_id'])
    
    db.session.add(transkrip)
    db.session.commit()

    return redirect(url_for('upload.edit_transkrip', transkrip_id=transkrip.id))



@upload.route('/edit_transkrip/<int:transkrip_id>', methods=['GET', 'POST'])
def edit_transkrip(transkrip_id):
    transkrip = Transkrip.query.get_or_404(transkrip_id)

    if request.method == 'POST':
        new_name = request.form.get('name')
        new_teks_html = request.form.get('teks')

        # Bersihkan HTML menjadi teks biasa
        plain_text = BeautifulSoup(new_teks_html or "", "html.parser").get_text()

        if new_name:
            transkrip.name = new_name
        if plain_text:
            transkrip.teks = plain_text
        
        transkrip.updated_at = datetime.now()
        db.session.commit()

        flash('Transkrip berhasil diperbarui', 'success')
        return redirect(url_for('upload.arsip_transkrip_page', transkrip_id=transkrip.id))

    return render_template('edit_transkrip.html', transkrip=transkrip)




@upload.route('/arsip-transkrip')
def arsip_transkrip_page():
    return render_template('arsip-transkrip.html')

@upload.route('/list-transkrip')
def list_transkrip():
    user_id = session.get('user_id')
    if not user_id:
        return redirect(url_for('auth.login'))  # Redirect jika belum login

    transkrip_list = Transkrip.query.filter_by(user_id=user_id).order_by(Transkrip.created_at.desc()).all()
    
    data = [{
        'id': t.id,
        'name': t.name,
        'created_at': t.created_at.strftime('%Y-%m-%d %H:%M:%S') if t.created_at else 'N/A',
        'preview': t.teks[:100].replace('\n', ' ') if t.teks else ''
    } for t in transkrip_list]

    return jsonify({'transkrip': data})





@upload.route('/soal')
def daftar_soal():
    semua_transkrip = Transkrip.query.order_by(Transkrip.created_at.desc()).all()
    return render_template('arsip-transkrip.html', transkrip_list=semua_transkrip)

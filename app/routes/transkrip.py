from flask import Blueprint, render_template, request, redirect, url_for, flash
from ..models import User, Transkrip, Summarize, Modul
from .. import db

transkrip_bp = Blueprint('transkrip', __name__)
@transkrip_bp.route('/edit/<int:transkrip_id>', methods=['GET', 'POST'])
def edit_transkrip(transkrip_id):
    transkrip = Transkrip.query.get_or_404(transkrip_id)

    if request.method == 'POST':
        new_text = request.form.get('teks', '').strip()

        if not new_text:
            flash('Teks transkrip tidak boleh kosong.', 'danger')
            return render_template('edit_transkrip.html', transkrip=transkrip)

        transkrip.teks = new_text
        db.session.commit()
        flash('Transkrip berhasil diperbarui!', 'success')
        return redirect(url_for('upload.arsip_transkrip_page'))

    return render_template('edit_transkrip.html', transkrip=transkrip)



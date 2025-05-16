from flask import Blueprint, render_template, request, redirect, url_for, session
from werkzeug.security import generate_password_hash, check_password_hash
from .. models import User
from .. import db

auth = Blueprint('auth', __name__)



@auth.route('/register', methods=['GET', 'POST'])
def register():
    if request.method == 'POST':
        username = request.form['username'] 
        email = request.form['email']
        password = request.form['password']

        hashed_password = generate_password_hash(password, method='pbkdf2:sha256')
        new_user = User(username=username, email=email, password=hashed_password)
        db.session.add(new_user)
        db.session.commit()

        return redirect(url_for('auth.login'))

    return render_template('register.html')

@auth.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        email = request.form['email']
        password = request.form['password']

        user = User.query.filter_by(email=email).first()
        if user and check_password_hash(user.password, password):
            session['user_id'] = user.id
            session['username'] = user.username
            return redirect(url_for('auth.dashboard'))
        else:
            return "Login gagal!"

    return render_template('index.html')

@auth.route('/')
def dashboard():
    if 'user_id' in session:
        print(session)  # cek apakah user_id dan username ada
        return render_template('dashboard.html')
    return redirect(url_for('auth.login'))

@auth.route('/logout')
def logout():
    session.clear()
    return redirect(url_for('auth.login'))

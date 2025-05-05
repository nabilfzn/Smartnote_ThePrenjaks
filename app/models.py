# from . import db


# # Membuat model database
# class User(db.Model):
#     id = db.Column(db.Integer, primary_key=True)
#     username = db.Column(db.String(150), unique=True, nullable=False)
#     email = db.Column(db.String(150), unique=True, nullable=False)
#     password = db.Column(db.String(150), nullable=False)


from . import db
from datetime import datetime
from flask_login import UserMixin

class User(UserMixin, db.Model):
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(80), unique=True, nullable=False)
    email = db.Column(db.String(120), unique=True, nullable=True)
    password = db.Column(db.String(200), nullable=False)  # hashed password
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    is_admin = db.Column(db.Boolean, default=False)

    # Relasi opsional
    transkrips = db.relationship('Transkrip', backref='owner', lazy=True, cascade="all, delete")
    summarizes = db.relationship('Summarize', backref='owner', lazy=True, cascade="all, delete")
    moduls = db.relationship('Modul', backref='owner', lazy=True, cascade="all, delete")


class Transkrip(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(255))
    teks = db.Column(db.Text)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)

    # foreign key ke user
    user_id = db.Column(db.Integer, db.ForeignKey('user.id'), nullable=False)
    
    # Relasi ke Modul & Summarize
    moduls = db.relationship('Modul', backref='parent_transkrip', lazy=True, cascade="all, delete")
    summarizes = db.relationship('Summarize', backref='parent_transkrip', lazy=True, cascade="all, delete")


class Summarize(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    data_summary = db.Column(db.Text)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)

    # foreign key ke user dan transkrip
    transkrip_id = db.Column(db.Integer, db.ForeignKey('transkrip.id'), nullable=False)
    user_id = db.Column(db.Integer, db.ForeignKey('user.id'), nullable=False)


class Modul(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    data_modul = db.Column(db.Text)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)

    # foreign key ke user dan transkrip
    transkrip_id = db.Column(db.Integer, db.ForeignKey('transkrip.id'), nullable=False)
    user_id = db.Column(db.Integer, db.ForeignKey('user.id'), nullable=False)

class Quiz(db.Model):
    __tablename__ = 'quizzes'
    id = db.Column(db.Integer, primary_key=True)
    title = db.Column(db.String(255))
    # Add the foreign key relationship to Modul
    modul_id = db.Column(db.Integer, db.ForeignKey('modul.id'), nullable=False)
    # Add relationship to access the parent Modul
    modul = db.relationship('Modul', backref=db.backref('quizzes', lazy=True))


class Question(db.Model):
    __tablename__ = 'questions'
    id = db.Column(db.Integer, primary_key=True)
    quiz_id = db.Column(db.Integer, db.ForeignKey('quizzes.id'))
    pertanyaan = db.Column(db.Text)
    kategori_taksonomi = db.Column(db.String(10))
    jawaban_benar = db.Column(db.String(1))
    penjelasan = db.Column(db.Text)

    quiz = db.relationship('Quiz', backref='questions')

class Choice(db.Model):
    __tablename__ = 'choices'
    id = db.Column(db.Integer, primary_key=True)
    question_id = db.Column(db.Integer, db.ForeignKey('questions.id'))
    label = db.Column(db.String(1))  # A, B, C, D, E
    text = db.Column(db.Text)

    question = db.relationship('Question', backref='choices')

class StudentAnswer(db.Model):
    __tablename__ = 'student_answers'  # Tambahkan __tablename__ untuk konsistensi nama tabel
    id = db.Column(db.Integer, primary_key=True)
    student_id = db.Column(db.String(100), nullable=False)  # Bisa disesuaikan jika kamu ingin menambahkan autentikasi
    question_id = db.Column(db.Integer, db.ForeignKey('questions.id'), nullable=False)  # Perbaiki di sini
    answer = db.Column(db.String(10), nullable=False)  # Jawaban yang dipilih siswa (A, B, C, D, E)
    correct = db.Column(db.Boolean, default=False)  # Menyimpan status apakah jawabannya benar atau salah
    timestamp = db.Column(db.DateTime, default=datetime.utcnow)

    question = db.relationship('Question', backref='student_answers')

    def __repr__(self):
        return f"<StudentAnswer {self.student_id} - Question {self.question_id}>"
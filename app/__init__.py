from flask import Flask
from flask_sqlalchemy import SQLAlchemy
import json, math, os, io, tempfile


db = SQLAlchemy()

def create_app():
    app = Flask(__name__)
    app.config['SECRET_KEY'] = '1234'
    app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///smartnotes1.db'
    app.config['UPLOAD_FOLDER'] = tempfile.gettempdir()
    app.config['MAX_CONTENT_LENGTH'] = 16 * 1024 * 1024  # 16MB max upload
    app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

    db.init_app(app)

    # Import blueprint dan register
    from .routes.auth import auth
    from .routes.transkrip import transkrip_bp
    from .routes.summarize import summarize_bp
    from .routes.modul import modul_bp
    from .routes.upload import upload
    from .routes.conn import conn
    from .routes.quiz import quizz

    app.register_blueprint(auth)
    app.register_blueprint(transkrip_bp)
    app.register_blueprint(summarize_bp)
    app.register_blueprint(modul_bp)
    app.register_blueprint(upload)
    app.register_blueprint(conn)
    app.register_blueprint(quizz)

    return app

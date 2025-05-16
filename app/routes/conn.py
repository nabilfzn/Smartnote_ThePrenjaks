from app import create_app
from flask import Flask, request, jsonify, send_from_directory, Blueprint, render_template
import os
import datetime
from flask_cors import CORS


conn = Blueprint('conn', __name__)


# Configuration
UPLOAD_FOLDER = 'app/static'
if not os.path.exists(UPLOAD_FOLDER):
    os.makedirs(UPLOAD_FOLDER)

@conn.route('/uploads', methods=['POST'])
def upload_file():
    """
    Endpoint to receive audio recordings from ESP32
    """
    try:
        # Check if the request has data
        if request.data:
            # Get the filename from headers or generate one
            content_disposition = request.headers.get('Content-Disposition', '')
            if 'filename=' in content_disposition:
                filename = content_disposition.split('filename=')[1].strip('"')
            else:
                # Generate filename with timestamp if none provided
                timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
                filename = f"recording_{timestamp}.wav"
            
            # Full path for saving the file
            filepath = os.path.join(UPLOAD_FOLDER, filename)
            
            # Save the file
            with open(filepath, 'wb') as f:
                f.write(request.data)
            
            file_size = os.path.getsize(filepath)
            
            print(f"File received and saved: {filepath} ({file_size} bytes)")
            
            # Return success response
            return jsonify({
                'status': 'success',
                'message': 'File uploaded successfully',
                'filename': filename,
                'size': file_size
            }), 200
        else:
            return jsonify({
                'status': 'error',
                'message': 'No data received'
            }), 400
    
    except Exception as e:
        print(f"Error during file upload: {str(e)}")
        return jsonify({
            'status': 'error',
            'message': f'Server error: {str(e)}'
        }), 500

@conn.route('/files', methods=['GET'])
def list_files():
    files = []
    for filename in os.listdir(UPLOAD_FOLDER):
        filepath = os.path.join(UPLOAD_FOLDER, filename)

        # Hanya proses file yang benar-benar ada
        if os.path.isfile(filepath):
            size = os.path.getsize(filepath)

            # Hapus file jika kosong
            if size == 0:
                print(f"File kosong terdeteksi, menghapus: {filename}")
                os.remove(filepath)
                continue  # Lewati file ini

            # Simpan file valid ke daftar
            modified = datetime.datetime.fromtimestamp(
                os.path.getmtime(filepath)
            ).strftime('%Y-%m-%d %H:%M:%S')
            
            files.append({
                'name': filename,
                'size': size,
                'modified': modified
            })

    return jsonify({'files': files})

@conn.route('/arsip')
def arsip_page():
    return render_template('arsip-rekaman.html')


@conn.route('/uploads/<filename>', methods=['GET'])
def download_file(filename):
    """
    Endpoint to download a specific file
    """
    return send_from_directory(UPLOAD_FOLDER, filename, as_attachment=True)

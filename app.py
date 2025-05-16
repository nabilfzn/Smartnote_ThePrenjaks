from app import create_app
import os

app = create_app()

if __name__ == '__main__':
    print(f"Server started. Recordings will be saved to {os.path.abspath('static')}")
    app.run(host='0.0.0.0', port=5055, debug=True)

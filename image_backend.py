import os
import uuid
import json
import threading
from datetime import datetime, timedelta

from flask import Flask, request, send_from_directory, jsonify, abort
from werkzeug.utils import secure_filename
from dotenv import load_dotenv

from openai import OpenAI

# —— 环境与配置 ——
load_dotenv()
UPLOAD_FOLDER = 'uploaded'
LINKS_FILE = 'temp_links.json'
EXPIRATION_MINUTES = 15
API_KEY = os.getenv("API_KEY")
DASHSCOPE_API_KEY = os.getenv("DASHSCOPE_API_KEY")
MODEL_NAME = "qwen-vl-max-latest"  # 可按需替换

# —— 初始化 Flask & OpenAI Client ——
app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

dashscope = OpenAI(
    api_key=DASHSCOPE_API_KEY,
    base_url="https://dashscope.aliyuncs.com/compatible-mode/v1",
)

# —— 链接持久化 helper ——
if not os.path.exists(LINKS_FILE):
    with open(LINKS_FILE, 'w') as f:
        json.dump({}, f)

def load_links():
    with open(LINKS_FILE, 'r') as f:
        return json.load(f)

def save_links(data):
    with open(LINKS_FILE, 'w') as f:
        json.dump(data, f)

# —— 清理过期图片线程 ——
def cleanup_expired_links():
    while True:
        links = load_links()
        now = datetime.utcnow()
        changed = False
        for key, info in list(links.items()):
            if datetime.fromisoformat(info['expires']) < now:
                # 删除文件并移除记录
                try: os.remove(os.path.join(UPLOAD_FOLDER, info['filename']))
                except FileNotFoundError: pass
                del links[key]
                changed = True
        if changed:
            save_links(links)
        threading.Event().wait(60)

threading.Thread(target=cleanup_expired_links, daemon=True).start()


# —— 上传接口 ——  
@app.route('/upload', methods=['POST'])
def upload_and_detect():
    # 1. 验证 API-KEY
    client_key = request.headers.get('X-API-KEY')
    if client_key != API_KEY:
        abort(401, description='Invalid API Key')

    # 2. 接收文件
    if 'image' not in request.files:
        abort(400, description='No image part')
    file = request.files['image']
    if file.filename == '':
        abort(400, description='No selected file')

    # 3. 存盘
    filename = secure_filename(file.filename)
    unique_id = uuid.uuid4().hex
    ext = os.path.splitext(filename)[1]
    new_filename = f"{unique_id}{ext}"
    file.save(os.path.join(app.config['UPLOAD_FOLDER'], new_filename))

    # 4. 生成临时随机路径
    path_id = uuid.uuid4().hex
    expires_at = (datetime.utcnow() + timedelta(minutes=EXPIRATION_MINUTES)).isoformat()
    links = load_links()
    links[path_id] = {'filename': new_filename, 'expires': expires_at}
    save_links(links)

    # 5. 构造可访问的图片 URL
    #    e.g. https://your.domain.com/image/<path_id>
    img_url = request.url_root.rstrip('/') + f"/image/{path_id}"

    # 6. 调用千问视觉模型进行检测
    prompt = (
        "请使用相对于画面归一化的矩形坐标，描述一下图片中输液包装袋的位置和大小，"
        "如果找到，输出格式需要符合std::cin，依次输出左上角x - 空格 - 左上角y - 空格 - 右下角x - 空格 - 右下角y，"
        "如果没找到，则全给0"
    )
    resp = dashscope.chat.completions.create(
        model=MODEL_NAME,
        messages=[
            {"role": "system", "content":[{"type":"text","text":"You are a helpful assistant."}]},
            {"role": "user", "content":[{"type":"image_url","image_url":{"url": img_url}}, {"type":"text","text": prompt}]},
        ],
    )
    raw = resp.choices[0].message.content.strip()
    # 解析 “x y x y” 或 “0 0 0 0”
    parts = raw.split()
    if len(parts) == 4 and all(p.replace('.', '', 1).isdigit() for p in parts):
        bbox = [float(p) for p in parts]
    else:
        bbox = [0.0, 0.0, 0.0, 0.0]

    # 7. 返回 JSON
    return jsonify({
        "url": img_url,
        "expires_at": expires_at,
        "bbox": bbox
    }), 200


# —— 图片访问接口 ——  
@app.route('/image/<path_id>', methods=['GET'])
def serve_image(path_id):
    links = load_links()
    if path_id not in links:
        abort(404)
    info = links[path_id]
    if datetime.utcnow() > datetime.fromisoformat(info['expires']):
        # 过期：删除并返回 410
        try: os.remove(os.path.join(UPLOAD_FOLDER, info['filename']))
        except FileNotFoundError: pass
        del links[path_id]; save_links(links)
        abort(410, description='Link expired')
    return send_from_directory(UPLOAD_FOLDER, info['filename'])


if __name__ == '__main__':
    # 开发环境下 HTTPS 示例（生产建议用 Nginx+Gunicorn+Certbot）
    # app.run(host='0.0.0.0', port=5000, ssl_context=('cert.pem','key.pem'))
    app.run(host='0.0.0.0', port=5000, debug=True)

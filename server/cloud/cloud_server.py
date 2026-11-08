#!/usr/bin/env python3
# cloud_server.py —— MQTT 收数 + SQLite 存储 + 网页曲线服务
import sqlite3, json, os, threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import paho.mqtt.client as mqtt

BROKER   = "192.168.68.129"
PORT     = 1883
USER     = "lxd"
PASSWORD = "040312"
CLIENT_ID = "cloud-receiver"

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DB_FILE  = os.path.join(BASE_DIR, "iot.db")   # 绝对路径，不受工作目录影响！
WEB_PORT = 8080

lock = threading.Lock()   # 两个线程都要访问数据库，加锁防冲突

def db_init():
    conn = sqlite3.connect(DB_FILE, check_same_thread=False)  # 允许跨线程用
    conn.execute("""CREATE TABLE IF NOT EXISTS sensor_data (
        id     INTEGER PRIMARY KEY AUTOINCREMENT,
        ts     TEXT DEFAULT (datetime('now','localtime')),
        topic  TEXT,
        payload TEXT
    )""")
    conn.commit()
    return conn

def on_connect(client, userdata, flags, rc):
    print(f"已连接 broker（返回码 {rc}）")
    client.subscribe("#")

def on_message(client, userdata, msg):
    payload = msg.payload.decode('utf-8', errors='replace')
    print(f"[收] {msg.topic} = {payload}")
    conn = userdata
    with lock:
        conn.execute("INSERT INTO sensor_data(topic, payload) VALUES (?,?)",
                     (msg.topic, payload))
        conn.commit()

class Handler(BaseHTTPRequestHandler):
    """HTTP 处理：/api/data 给数据，/ 给网页"""
    def do_GET(self):
        if self.path == "/api/data":
            with lock:
                conn = sqlite3.connect(DB_FILE)
                rows = conn.execute("""
                    SELECT ts, payload FROM sensor_data
                    WHERE topic = '开发板温度信息'
                    ORDER BY id DESC LIMIT 50
                """).fetchall()
                conn.close()
            # 倒序转正序，再把 "44132" 转成 44.132
            data = [{"ts": r[0][11:19], "temp": float(r[1]) / 1000} for r in reversed(rows)]
            body = json.dumps(data, ensure_ascii=False).encode('utf-8')
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif self.path in ("/", "/index.html"):
            path = os.path.join(BASE_DIR, "index.html")
            body = open(path, 'rb').read()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        else:
            self.send_response(404); self.end_headers()

def main():
    conn = db_init()
    client = mqtt.Client(client_id=CLIENT_ID)
    client.username_pw_set(USER, PASSWORD)
    client.user_data_set(conn)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT, 60)
    client.loop_start()          # 后台线程收消息（不阻塞主线程）

    srv = ThreadingHTTPServer(("0.0.0.0", WEB_PORT), Handler)
    print(f"MQTT 已启动，网页打开： http://192.168.68.129:{WEB_PORT}")
    print("按 Ctrl+C 退出")
    srv.serve_forever()

if __name__ == "__main__":
    main()

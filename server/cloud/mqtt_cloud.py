#!/usr/bin/env python3
# mqtt_cloud.py —— 云端数据接收：订阅 broker → 打印 + 存入 SQLite
# 对应原项目 cloud/mqtt_to_influxdb.py 的简化版
import sqlite3
import paho.mqtt.client as mqtt

BROKER   = "192.168.68.129"     # broker 地址（就是虚拟机自己）
PORT     = 1883
USER     = "lxd"
PASSWORD = "040312"             # ← 和你板子 mqtt.c 里 conn_opts.password 一致
CLIENT_ID = "cloud-receiver"    # 客户端ID，不能和板子 smart_home 重复
DB_FILE  = "iot.db"             # SQLite 数据库文件名

def db_init():
    """建表：id / 时间 / 主题 / 内容"""
    conn = sqlite3.connect(DB_FILE)
    conn.execute("""CREATE TABLE IF NOT EXISTS sensor_data (
        id     INTEGER PRIMARY KEY AUTOINCREMENT,
        ts     TEXT DEFAULT (datetime('now','localtime')),
        topic  TEXT,
        payload TEXT
    )""")
    conn.commit()
    return conn

def on_connect(client, userdata, flags, rc):
    """连接成功回调：订阅所有主题"""
    print(f"已连接 broker（返回码 {rc}）")
    client.subscribe("#")        # "#" = MQTT 通配符，订阅所有主题

def on_message(client, userdata, msg):
    """收到消息回调：打印 + 入库"""
    payload = msg.payload.decode('utf-8', errors='replace')   # 字节 → 字符串
    print(f"[收] {msg.topic} = {payload}")

    conn = userdata              # 取回数据库连接
    conn.execute("INSERT INTO sensor_data(topic, payload) VALUES (?,?)",
                 (msg.topic, payload))
    conn.commit()

def main():
    conn = db_init()

    client = mqtt.Client(client_id=CLIENT_ID)
    client.username_pw_set(USER, PASSWORD)
    client.user_data_set(conn)              # 把数据库连接挂在 userdata 上，回调里取
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(BROKER, PORT, 60)
    client.loop_forever()                   # 阻塞收消息（和板子 receive 一个道理）

if __name__ == "__main__":
    main()

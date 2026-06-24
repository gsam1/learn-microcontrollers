import sys
import os
import paho.mqtt.client as mqtt
from datetime import datetime

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from config import MQTT_BROKER, MQTT_PORT, MQTT_TOPIC, MQTT_USER, MQTT_PASSWORD

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"Connected to {MQTT_BROKER}:{MQTT_PORT}")
        print(f"Subscribed to {MQTT_TOPIC}\n")
        client.subscribe(MQTT_TOPIC)
    else:
        print(f"Failed to connect, return code {rc}")

def on_message(client, userdata, msg):
    ts = datetime.now().strftime("%H:%M:%S")
    print(f"[{ts}] {msg.payload.decode()}")

def on_disconnect(client, userdata, rc):
    print("Disconnected from MQTT broker")

client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect

try:
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_forever()
except KeyboardInterrupt:
    print("\nDisconnecting...")
    client.disconnect()
except Exception as e:
    print(f"Error: {e}")
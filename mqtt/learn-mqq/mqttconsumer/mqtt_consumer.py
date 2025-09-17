import paho.mqtt.client as mqtt

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker")
        client.subscribe("pico/sensor_data")
    else:
        print(f"Failed to connect, return code {rc}")

def on_message(client, userdata, msg):
    print(f"Received message: {msg.payload.decode()} on topic: {msg.topic}")

def on_disconnect(client, userdata, rc):
    print("Disconnected from MQTT broker")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect

try:
    client.connect("localhost", 1883, 60)
    client.loop_forever()
except KeyboardInterrupt:
    print("\nDisconnecting...")
    client.disconnect()
except Exception as e:
    print(f"Error: {e}")
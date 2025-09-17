import network
import time
from machine import Pin, UART
from umqtt_simple import MQTTClient
import utime
import binascii

# Import your sensor library here, e.g., from dht import DHT11

# Wi-Fi and MQTT configuration
WIFI_SSID = "SAMWIFI"
WIFI_PASSWORD = "*****"
MQTT_BROKER = "192.168.1.75"
MQTT_PORT = 1883
MQTT_TOPIC = "pico/sensor_data"


class MMRadar:
    def __init__(self) -> None:
        tx_pin_number = 4
        rx_pin_number = 5

        self.tx_pin = Pin(tx_pin_number)
        self.rx_pin = Pin(rx_pin_number)

    def send_hex_string(self):
        hex_string = "FDFCFBFA0800120000006400000004030201"
        hex_bytes = binascii.unhexlify(hex_string)
    
    def read_serial_data(self, mqtt_client):
        soft_uart = UART(1, baudrate=115200, tx=self.tx_pin, rx=self.rx_pin)
        print("Reading serial data...")
        buffer = []
        counter = 0
        while True:
            if soft_uart.any():
                data = soft_uart.readline()
                if data == b'ON\r\n':
                    buffer.append(1)
                elif data == b'OFF\r\n':
                    buffer.append(0)
                
                if len(buffer) >= 10:
                    buffer_sum = sum(buffer)
                    if buffer_sum >= 3:
                        payload = "Presence Detected." + str(buffer_sum) + str(counter)
                        try:
                            print(payload)
                            mqtt_client.publish(MQTT_TOPIC, payload)
                        except Exception as e:
                            print("Failed to publish message:", e)
                            mqtt_client.reconnect()
                    else:
                        try:
                            payload = "No Presence Detected." + str(buffer_sum) + str(counter)
                            print(payload)
                            mqtt_client.publish(MQTT_TOPIC, payload)
                        except Exception as e:
                            print("Failed to publish message:", e)
                            mqtt_client.reconnect()

                    counter += 1
                    buffer = []

    
# Function to connect to Wi-Fi
def connect_to_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(WIFI_SSID, WIFI_PASSWORD)
    while not wlan.isconnected():
        print("Waiting for Wi-Fi connection...")
        time.sleep(1)
    print("Connected to Wi-Fi!")

# Function to connect to MQTT broker
def connect_to_mqtt():
    client = MQTTClient(
        client_id=b"pico_client",
        server=MQTT_BROKER,
        port=MQTT_PORT
    )
    client.connect()
    print("Connected to MQTT Broker!")
    return client

# Main loop
def main():
    connect_to_wifi()
    mqtt_client = connect_to_mqtt()

    mmradar = MMRadar()
    mmradar.send_hex_string()
    mmradar.read_serial_data(mqtt_client)

if __name__ == "__main__":
    main()





def send_hex_string(hex_string):
    hex_bytes = binascii.unhexlify(hex_string)
    soft_uart.write(hex_bytes)


               

if __name__ == "__main__":
    hex_to_send = "FDFCFBFA0800120000006400000004030201"
    send_hex_string(hex_to_send)
    response = read_serial_data()
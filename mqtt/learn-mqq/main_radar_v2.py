import network
import time
import binascii
from machine import Pin, UART
from umqtt_simple import MQTTClient
from config import WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER, MQTT_PORT, MQTT_TOPIC, MQTT_USER, MQTT_PASSWORD


class MMRadar:
    def __init__(self) -> None:
        # Sensor TX -> GPIO 5 (Pico RX), Sensor RX -> GPIO 4 (Pico TX)
        self.uart = UART(1, baudrate=115200, tx=Pin(4), rx=Pin(5), timeout=500)
        self._send_init()

    def _send_init(self):
        self.uart.write(binascii.unhexlify("FDFCFBFA0800120000006400000004030201"))
        time.sleep_ms(100)
        print("Init sent")

    def read_serial_data(self, mqtt_client):
        print("Reading serial data...")
        buffer = []
        counter = 0

        while True:
            line = self.uart.readline()
            if line is None:
                continue

            line = line.strip()
            if line == b"ON":
                buffer.append(1)
            elif line == b"OFF":
                buffer.append(0)
            else:
                continue  # skip Range lines

            if len(buffer) >= 10:
                buffer_sum = sum(buffer)
                if buffer_sum >= 3:
                    payload = "Presence Detected." + str(buffer_sum) + "." + str(counter)
                else:
                    payload = "No Presence Detected." + str(buffer_sum) + "." + str(counter)
                try:
                    print(payload)
                    mqtt_client.publish(MQTT_TOPIC, payload)
                except Exception as e:
                    print("Failed to publish:", e)
                    mqtt_client.reconnect()

                counter += 1
                buffer = []


def connect_to_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(WIFI_SSID, WIFI_PASSWORD)
    while not wlan.isconnected():
        print("Waiting for Wi-Fi connection...")
        time.sleep(1)
    print("Connected to Wi-Fi!")


def connect_to_mqtt():
    client = MQTTClient(
        client_id=b"pico_client",
        server=MQTT_BROKER,
        port=MQTT_PORT,
        user=MQTT_USER,
        password=MQTT_PASSWORD
    )
    client.connect()
    print("Connected to MQTT Broker!")
    return client


def main():
    connect_to_wifi()
    mqtt_client = connect_to_mqtt()
    mmradar = MMRadar()
    mmradar.read_serial_data(mqtt_client)


if __name__ == "__main__":
    main()

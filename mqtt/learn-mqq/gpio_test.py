from machine import Pin
import time

# Connect sensor OT2 pin to Pico GPIO 2
ot2 = Pin(2, Pin.IN)

print("Watching OT2 pin — wave your hand in front of the sensor...")
prev = None
while True:
    val = ot2.value()
    if val != prev:
        print("OT2:", "DETECTED" if val else "CLEAR")
        prev = val
    time.sleep_ms(50)

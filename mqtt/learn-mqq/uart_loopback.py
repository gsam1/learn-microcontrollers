from machine import Pin, UART
import time

uart = UART(1, baudrate=115200, tx=Pin(4), rx=Pin(5), timeout=200)

uart.write(b"hello")
time.sleep_ms(50)

data = uart.read(32)
if data:
    print("Loopback OK:", data)
else:
    print("Loopback FAILED — nothing received back")

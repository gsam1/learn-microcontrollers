from machine import Pin, UART
import binascii
import time

uart = UART(1, baudrate=115200, tx=Pin(4), rx=Pin(5), timeout=500)

# Send init command before listening
hex_string = "FDFCFBFA0800120000006400000004030201"
uart.write(binascii.unhexlify(hex_string))
print("Init sent, listening for 15 seconds...")

start = time.ticks_ms()
while time.ticks_diff(time.ticks_ms(), start) < 15000:
    data = uart.read(32)
    if data:
        print("HEX:", data.hex())

print("Done.")

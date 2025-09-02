from machine import Pin
import time

# Define the onboard LED pin
# The Pico W's onboard LED is connected to GPIO pin 25
led = Pin("LED", Pin.OUT)
led.toggle()

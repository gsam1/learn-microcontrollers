from machine import Pin, I2C, ADC
import framebuf
import time
from neopixel import NeoPixel


class Audio:
    def __init__(self, voltage_threshold = 1):
        self.voltage_threshold = voltage_threshold
        sound_aout_num = 28
        self.sound_aout = ADC(Pin(sound_aout_num))
        sound_dout_num = 21
        self.sound_dout = Pin(sound_dout_num,Pin.IN)    
    
    def check_audio(self):
        voltage = self.sound_aout.read_u16()*3.3/65535
        print(voltage)
        if voltage > self.voltage_threshold:
            return True
        else:
            return False
        
def main():
    # Create an Audio object with a voltage threshold.
    audio = Audio(voltage_threshold=1)
    
    # Define the pin for the NeoPixel LED and initialize it.
    led_num = 10
    led = Pin(led_num,Pin.OUT)
    print("led demo")

    

    while True:
        # Check if audio is detected.
        if audio.check_audio():
            print("DETECTED AUDIO")
            # If audio is detected, turn the LED blue.
            # The NeoPixel color tuple is (R, G, B), so blue is (0, 0, 255).
            # led.on()
            led.toggle()
        else:
            led.off()
        
        # A small delay to prevent the loop from running too fast.
        time.sleep_ms(300)


if __name__ == '__main__':
    main()
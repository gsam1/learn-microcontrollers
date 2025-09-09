import machine
import utime
import binascii

tx_pin_number = 4
rx_pin_number = 5

tx_pin = machine.Pin(tx_pin_number)
rx_pin = machine.Pin(rx_pin_number)

soft_uart = machine.UART(1, baudrate=115200, tx=tx_pin, rx=rx_pin)

def send_hex_string(hex_string):
    hex_bytes = binascii.unhexlify(hex_string)
    soft_uart.write(hex_bytes)

def read_serial_data():
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
                    print("Presence Detected.", buffer_sum, counter)
                else:
                    print("No Presence Detected.", buffer_sum, counter)
                
                counter += 1
                buffer = []
               
            # if data != b'ON\r\n':
            #     if len(buffer) <= 5:
            #         buffer.append(data)
            #     else:
            #         new_buffer = []
            #         for item in buffer:
            #             try:
            #                 new_item = item.decode('utf-8')
            #                 new_buffer.append(new_item)
            #             except Exception:
            #                 pass
            #         new_buffer.append(counter)
            #         counter += 1
            #         print(new_buffer)
            #         buffer = []
            #         buffer.append(data)

            # utime.sleep_ms(100) 

    

if __name__ == "__main__":
    hex_to_send = "FDFCFBFA0800120000006400000004030201"
    send_hex_string(hex_to_send)
    response = read_serial_data()
    
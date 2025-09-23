import machine
import binascii

txa_pin_number = 0
rxa_pin_number = 1
txb_pin_number = 4
rxb_pin_number = 5


txa_pin = machine.Pin(txa_pin_number)
rxa_pin = machine.Pin(rxa_pin_number)
txb_pin = machine.Pin(txb_pin_number)
rxb_pin = machine.Pin(rxb_pin_number)


soft_uart_a = machine.UART(0, baudrate=115200, tx=txa_pin, rx=rxa_pin)
soft_uart_b = machine.UART(1, baudrate=115200, tx=txb_pin, rx=rxb_pin)

def send_hex_string(hex_string):
    hex_bytes = binascii.unhexlify(hex_string)
    print("Sending to a")
    soft_uart_a.write(hex_bytes)
    print("Sending to b")
    soft_uart_b.write(hex_bytes)


def read_serial_data():
    print("Reading serial data...")
    while True:
        if soft_uart_a.any():
            dataa = soft_uart_a.read()
            print(dataa)
        
        if soft_uart_b.any():
            datab = soft_uart_b.read()
            print(datab)
            # data = dataa + datab
            # if data == b'ON\r\n':
            #     buffer.append(1)
            # elif data == b'OFF\r\n':
            #     buffer.append(0)
            
            # if len(buffer) >= 10:
            #     buffer_sum = sum(buffer)
            #     if buffer_sum >= 3:
            #         print("Presence Detected.", buffer_sum, counter)
            #     else:
            #         print("No Presence Detected.", buffer_sum, counter)
                
            #     counter += 1
            #     buffer = []
               

if __name__ == "__main__":
    hex_to_send = "FDFCFBFA0800120000006400000004030201"
    send_hex_string(hex_to_send)
    response = read_serial_data()
    
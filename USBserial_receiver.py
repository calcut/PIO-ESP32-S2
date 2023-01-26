import serial
import serial.tools.list_ports
import binascii
import time


ports = serial.tools.list_ports.comports()
metro_port = None

for port, desc, hwid in sorted(ports):
    if desc == "Metro ESP32-S2":
        metro_port = port

if not metro_port:
    print("Could not find metro port, available ports are:")
    n =1 
    for port, desc, hwid in sorted(ports):
        print(f"{n}) {port}: {desc} [{hwid}]")
        n+=1
    exit()

ser = serial.Serial(metro_port)  # open serial port
print(f'listening on {port}...')
ser.reset_input_buffer()
data = b""
while True:
    try:
        buffer = ser.readline()
        if buffer[:16] == b"Incomingbytes = ":
            print(f'{buffer=}')
            bytes_to_read = int(buffer[16:-1])
            print(f'{bytes_to_read=}')
            data = ser.read(bytes_to_read)
            
        
        elif buffer[:13] == b"checksum = 0x":

            filename = buffer[35:-1].decode("ascii")
            checksum_remote = hex(int(buffer[11:21], 16))
            checksum_local = hex(binascii.crc32(data))
            print(f'{len(data)=}')

            if checksum_local == checksum_remote:
                print(f'File {filename} received successfully')
                with open(f"./{filename}", 'wb') as f:
                    f.write(data)
            else:
                print('checksum error!')
                print(f'{checksum_local=}')
                print(f'{checksum_remote=}')
                # TODO request the file again?
            data = b""
        # else:
        #     data += buffer[:-1]
            # print(f'Received data packet, {len(buffer[:-1])} bytes')

    except OSError as e:
        # print(f'{e.args=}')
        print('Port lost, trying to reconnect...')
        time.sleep(1)
        try:
            ser = serial.Serial(metro_port)
            ser.reset_input_buffer()
        except:
            print('failed again, waiting 10s')
            time.sleep(10)




import serial
import serial.tools.list_ports
import binascii
import time
import os

def download_file(filename):
    # TODO ADD timeout
    print(f"Downloading {filename}...")
    start = time.time()
    ser.write(bytes(f"<cp /{filename}>", "ascii"))
    while True:
        buffer = ser.readline()
        if buffer[:16] == b"Incomingbytes = ":
            bytes_to_read = int(buffer[16:-1])
            data = ser.read(bytes_to_read)
            
        
        elif buffer[:13] == b"checksum = 0x":

            filename = buffer[35:-1].decode("ascii")
            checksum_remote = hex(int(buffer[11:21], 16))
            checksum_local = hex(binascii.crc32(data))

            if checksum_local == checksum_remote:
                end = time.time()
                kbs = round(len(data)/1024,1)
                rate = round(kbs/(end-start),1)
                elapsed = round(end-start,1)
                print(f'File {filename} received successfully, {kbs}KB in {elapsed}s, {rate}KB/s')
                with open(f"./Received/{filename}", 'wb') as f:
                    f.write(data)
                ser.write(bytes(f"<rm /{filename}>", "ascii"))
                
                return
            else:
                print('checksum error! please try again')
                print(f'{checksum_local=}')
                print(f'{checksum_remote=}')
                # TODO request the file again?
                return 


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

print(f'requesting SDCARD ls')
ser.write(bytes("<ls>", "ascii"))
time.sleep(5)
remote_files = []
while ser.in_waiting:
    data = ser.readline()
    remote_files.append(data[1:-1].decode('ascii'))

print(f'{remote_files=}')

local_files = os.listdir("./Received")
print(f'{local_files=}')

for f in remote_files:
    if f not in local_files:
        print(f'About to download {f}')
        download_file(f)






# while True:
#     try:
#         buffer = ser.readline()
#         if buffer[:16] == b"Incomingbytes = ":
#             print(f'{buffer=}')
#             bytes_to_read = int(buffer[16:-1])
#             print(f'{bytes_to_read=}')
#             data = ser.read(bytes_to_read)
            
        
#         elif buffer[:13] == b"checksum = 0x":

#             filename = buffer[35:-1].decode("ascii")
#             checksum_remote = hex(int(buffer[11:21], 16))
#             checksum_local = hex(binascii.crc32(data))
#             print(f'{len(data)=}')

#             if checksum_local == checksum_remote:
#                 print(f'File {filename} received successfully')
#                 with open(f"./Received/{filename}", 'wb') as f:
#                     f.write(data)
#             else:
#                 print('checksum error!')
#                 print(f'{checksum_local=}')
#                 print(f'{checksum_remote=}')
#                 # TODO request the file again?
#             data = b""
#         # else:
#         #     data += buffer[:-1]
#             # print(f'Received data packet, {len(buffer[:-1])} bytes')

#     except OSError as e:
#         # print(f'{e.args=}')
#         print('Port lost, trying to reconnect...')
#         time.sleep(1)
#         try:
#             ser = serial.Serial(metro_port)
#             ser.reset_input_buffer()
#         except:
#             print('failed again, waiting 10s')
#             time.sleep(10)




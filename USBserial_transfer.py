import serial
import serial.tools.list_ports
import binascii
import time
import os
import numpy as np
from obspy import UTCDateTime, read, Trace, Stream
import struct

def hexstring_to_miniseed(filename, directory, stats=None):

    outfile = os.path.splitext(filename)[0] +".mseed"
    path_in = os.path.join(directory, filename)
    # path_out = os.path.join(directory, outfile)
    path_out = os.path.join("/Users/calum/", outfile)

    s0 = np.array([], dtype=np.dtype("f"))
    s1 = np.array([], dtype=np.dtype("f"))
    s2 = np.array([], dtype=np.dtype("f"))
    stamps = np.array([], dtype=np.dtype("f"))

    with open(path_in, "r") as f:
        for line in f:
            # print(f'{line[0:8]+line[9:17]+line[18:26]=}')
            data_bytes = bytes.fromhex(line[0:8]+line[9:17]+line[18:26])
            stamp = float(f"{line[27:37]}.{line[38:45]}")
            dec = struct.unpack(">iii", data_bytes)
            s0 = np.append(s0, dec[0])
            s1 = np.append(s1, dec[1])
            s2 = np.append(s2, dec[2])
            stamps = np.append(stamps, stamp)

    end = UTCDateTime(stamps[-1])
    start = UTCDateTime(stamps[0])

    elapsed = end - start
    npts = len(stamps)
    freq = 1/(elapsed/(npts-1))

    if stats is None:
        stats = {'network': 'BW', 'station': 'RJOB', 'location': '',
                'channel': 'WLZ', 'npts': len(s0), 'sampling_rate': 250,
                'mseed': {'dataquality': 'D'}}

    stats['starttime'] = UTCDateTime(stamps[0])

    stats0 = stats.copy()
    stats1 = stats.copy()
    stats2 = stats.copy()
    stats0['channel'] = "CH0"
    stats1['channel'] = "CH1"
    stats2['channel'] = "CH2"


    st = Stream([Trace(data=s0, header=stats0), Trace(data=s1, header=stats1), Trace(data=s2, header=stats2)])


    try:
        st_existing = read(path_out)
        for s in st_existing:
            st.append(s)
        st.merge(method=0, fill_value=0)
        # print("merging")
    except FileNotFoundError:
        pass
    st.write(path_out, format='MSEED')


def download_file(filename, outdir):
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

                path = os.path.join(outdir,filename)
                with open(path, 'wb') as f:
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

WORKING_DIR = "./Received"

ser = serial.Serial(metro_port)  # open serial port
print(f'listening on {port}...')


while True:
    ser.reset_input_buffer()
    data = b""

    print(f'requesting SDCARD ls')
    ser.write(bytes("<ls>", "ascii"))
    time.sleep(2)
    remote_files = []
    while ser.in_waiting:
        data = ser.readline()
        remote_files.append(data[1:-1].decode('ascii'))

    print(f'{remote_files=}')

    local_files = os.listdir(WORKING_DIR)
    print(f'{local_files=}')

    for f in remote_files:
        # if f not in local_files:
        if f[0] != ".":
            print(f'About to download {f}')
            download_file(f, WORKING_DIR)
            try:
                hexstring_to_miniseed(f, WORKING_DIR)
            except Exception as e:
                print(f"Error converting {f}, {e}")
    
    time.sleep(5)
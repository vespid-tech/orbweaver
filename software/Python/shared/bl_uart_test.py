import serial
import time

UART_PORT = "COM6"  # Raspberry Pi UART port (adjust as needed)
UART_BAUD = 9600

    
ser = serial.Serial(port=UART_PORT, baudrate=UART_BAUD, bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, timeout=1)

ser.reset_input_buffer()
ser.reset_output_buffer()
time.sleep(0.01)  # Brief 10ms rest


attempts = 0
while True:

    print(attempts)
    attempts += 1
    
    # Send the sync frame
    ser.write(b'\x55\x55')
    
    # Read the response (up to 1 byte)
    response = ser.read(1)
    
    if response == b'\xcc':
        print(f"\n[!] Success! Got ACK (0xCC) after {attempts} attempts.")
        break
        
    # Optional indicator to show the script is working in real time
    print(".", end="", flush=True) 
    
    # Short micro-delay to avoid saturating the Holtek UART bridge buffer
    time.sleep(0.01)


# CRITICAL PAUSE: Give the Holtek chip and CC1310 time to finish processing the lock sequence
time.sleep(0.05)

# Clear out any leftover bits stuck in transitions
ser.reset_input_buffer()
ser.reset_output_buffer()
time.sleep(0.05)

# 3. Verify communication at 9600 by sending a standard PING frame
print("Verifying link at 9600 Baud with a PING command...")
ser.write(b'\x03\x20\x20')
verification = ser.read(1)

if verification == b'\xcc':
    print("[+] Link established at 9600 Baud successfully!")
else:
    print(f"[-] Reconfiguration failed. Expected 0xCC, got: {verification.hex().upper() or 'Timeout'}")

ser.close()
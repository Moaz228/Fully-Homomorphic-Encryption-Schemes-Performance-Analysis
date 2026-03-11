import subprocess
import os
import json
import base64
import paho.mqtt.client as mqtt
from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad

# ================= CONFIGURATION =================
# 1. Choose your scheme here: "BFV", "BGV", or "CKKS"
SELECTED_SCHEME = "CKKS" 

# 2. MQTT Settings (Coordinate these with your ESP32 friend)
MQTT_BROKER = "broker.hivemq.com" # Using a public broker for testing
MQTT_PORT = 1883
MQTT_TOPIC = "grad_project/sensor_data"

# 3. AES Settings (Must match ESP32)
AES_KEY = b'my_super_secret1' 

# Mapping schemes to their binaries
ENGINES = {
    "BFV": "./build/bin.out",
    "BGV": "./build/bgv_bin.out",
    "CKKS": "./build/ckks_bin.out"
}
# =================================================

def decrypt_aes(payload_b64):
    """ Responsibility: AES Decryption """
    try:
        raw_data = base64.b64decode(payload_b64)
        iv = raw_data[:16]
        ciphertext = raw_data[16:]
        
        cipher = AES.new(AES_KEY, AES.MODE_CBC, iv)
        decrypted = unpad(cipher.decrypt(ciphertext), AES.block_size)
        
        # Convert the decrypted string (e.g. "2543,2543...") to list
        return [int(x) for x in decrypted.decode('utf-8').split(',')]
    except Exception as e:
        print(f"[!] AES Decryption failed: {e}")
        return None

def on_message(client, userdata, msg):
    print(f"\n[MQTT] Received message on {msg.topic}")
    
    # 1. Decrypt AES
    readings = decrypt_aes(msg.payload)
    
    if readings:
        print(f"[Python] Decrypted {len(readings)} readings. Writing to file...")
        
        # 2. Prepare file for C++
        with open("sensor_readings.txt", "w") as f:
            for val in readings:
                f.write(f"{val}\n")
        
        # 3. Trigger chosen FHE Engine
        engine_path = ENGINES[SELECTED_SCHEME]
        print(f"[Python] Launching {SELECTED_SCHEME} Engine...")
        
        result = subprocess.run([engine_path], capture_output=True, text=True)
        print("\n--- FHE ENGINE OUTPUT ---")
        print(result.stdout)
        print("-------------------------")
    else:
        print("[!] No valid data to process.")

# Setup MQTT Client
client = mqtt.Client()
client.on_message = on_message

print(f"--- FHE Gateway: {SELECTED_SCHEME} Mode ---")
print(f"[*] Connecting to Broker: {MQTT_BROKER}...")
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.subscribe(MQTT_TOPIC)

print(f"[*] Subscribed to: {MQTT_TOPIC}")
print("[*] Waiting for ESP32 data... (Press Ctrl+C to stop)")
client.loop_forever()
import base64
import glob
import os
import subprocess
import time

import paho.mqtt.client as mqtt
import requests
from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad

# ================= CONFIGURATION =================
SELECTED_SCHEME = "CKKS"
CLOUD_URL = "http://localhost:5000/compute/add"  # Set your operation here

MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "grad_project/sensor_data"

AES_KEY = b"my_super_secret1"

ENGINES = {
    "BFV": "./build/bfv_bin.out",
    "BGV": "./build/bgv_bin.out",
    "CKKS": "./build/ckks_bin.out",
}
# =================================================


def decrypt_aes(payload_b64):
    try:
        start_time = time.time()
        raw_data = base64.b64decode(payload_b64)
        iv = raw_data[:16]
        ciphertext = raw_data[16:]
        cipher = AES.new(AES_KEY, AES.MODE_CBC, iv)
        decrypted = unpad(cipher.decrypt(ciphertext), AES.block_size)
        end_time = time.time()
        print(f"AES Decryption Time: {(end_time - start_time)*1000:.4f} ms")
        return [float(x) for x in decrypted.decode("utf-8").split(",")]
    except Exception as e:
        print(f"[!] AES Decryption failed: {e}")
        return None


def on_message(client, userdata, msg):
    print(f"\n[MQTT] Received data from ESP32...")

    # 1. AES Decrypt
    readings = decrypt_aes(msg.payload)
    if not readings:
        return

    # 2. Write for C++
    with open("sensor_readings.txt", "w") as f:
        for val in readings:
            f.write(f"{val}\n")

    # 3. Trigger C++ ENCRYPTION
    engine_path = ENGINES[SELECTED_SCHEME]
    print(f"[*] Running {SELECTED_SCHEME} Encryption...")
    subprocess.run([engine_path], check=True)  # Runs C++ to create .bin files

    # 4. Upload to Flask Cloud
    print(f"[*] Sending encrypted data to Cloud API ({CLOUD_URL})...")
    try:
        # We send context, data, and the evaluation keys
        files = {
            "context": open("cryptocontext.bin", "rb"),
            "data": open("ciphertext_out.bin", "rb"),
            "mult_key": open("mult_key.bin", "rb"),
            "rot_key": open("rot_key.bin", "rb"),
        }
        print((os.path.getsize("ciphertext_out.bin")) / 1024)
        start_time = time.time()
        response = requests.post(CLOUD_URL, files=files)
        end_time = time.time()
        print(f"Cloud response time: {(end_time - start_time)*1000:.4f} ms")
        if response.status_code == 200:
            # 5. Save the Cloud's result
            with open("ciphertext_in.bin", "wb") as f:
                f.write(response.content)
            print("[+] Cloud computation complete. Result saved.")

            # 6. Trigger C++ DECRYPTION
            # Note: We pass '--decrypt' so your C++ code knows to switch modes
            print("[*] Running FHE Decryption...")
            start_time = time.time()
            result = subprocess.run(
                [engine_path, "--decrypt"], capture_output=True, text=True
            )
            end_time = time.time()
            print(
                f"{SELECTED_SCHEME} Decryption Time: {(end_time - start_time)*1000:.4f} ms"
            )

            print("\n--- FINAL FHE RESULTS ---")
            print(result.stdout)
            print("-------------------------")
        else:
            print(f"[!] Cloud Error: {response.text}")

    except Exception as e:
        print(f"[!] Request failed: {e}")


# Setup MQTT Client
client = mqtt.Client()
client.on_message = on_message

print("Clearing old Data...")
for f in [
    "cryptocontext.bin",
    "public_key.bin",
    "secret_key.bin",
    "mult_key.bin",
    "rot_key.bin",
    "ciphertext_in.bin",
    "ciphertext_out.bin",
]:
    if os.path.exists(f):
        os.remove(f)

print(f"--- FHE Gateway: {SELECTED_SCHEME} Mode ---")
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.subscribe(MQTT_TOPIC)
client.loop_forever()

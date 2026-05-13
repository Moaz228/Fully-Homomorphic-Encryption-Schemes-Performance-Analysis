import base64
import csv
import os
import re
import subprocess
import time

import paho.mqtt.client as mqtt
import psutil
import requests
from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad

# ================= CONFIGURATION =================
WORKSTATION_IP = "localhost"  # Set to actual IP when moving to Pi

# Schemas: [CKKS,BFV,BGVG
# Ops: [add,multiply,average,oldAverage]
SELECTED_SCHEME = os.environ.get("SELECTED_SCHEME", "CKKS")
OPERATION = os.environ.get("SELECTED_OPERATION", "multiply")
CLOUD_URL = f"http://{WORKSTATION_IP}:5000/compute/{OPERATION}"

MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "grad_project/sensor_data"

AES_KEY = b"my_super_secret1"
# CSV_FILE = "fog_performance_log.csv"

ENGINES = {
    "BFV": "./build/bfv_bin.out",
    "BGV": "./build/bgv_bin.out",
    "CKKS": "./build/ckks_bin.out",
}

# ================= GLOBAL STATE =================
state = {"v1_received": False, "metrics": {}}


def reset_metrics_dict():
    return {
        "Timestamp": "",
        "Schema": SELECTED_SCHEME,
        "Operation": OPERATION,
        "v1_plaintext_size": "",
        "v2_plaintext_size": "",
        "v1_ciphertext_size": "",
        "v2_ciphertext_size": "",
        "v1_aes_dec_ms": "",
        "v2_aes_dec_ms": "",
        "v1_enc_kb": 0.0,
        "v2_enc_kb": 0.0,
        "v1_enc_cpu": "",
        "v2_enc_cpu": "",
        "v1_fhe_enc_ms": "",
        "v2_fhe_enc_ms": "",
        "fhe_dec_ms": "",
        "fhe_dec_kb": "",
        "fhe_dec_cpu": "",
        "cloud_compute_ms": "",
    }


# ================= HELPERS =================


def parse_time_v(stderr_output):
    metrics = {}
    mem_match = re.search(
        r"Maximum resident set size \(kbytes\): (\d+)", stderr_output
    )
    cpu_match = re.search(r"Percent of CPU this job got: (\d+)%", stderr_output)
    metrics["kb"] = mem_match.group(1) if mem_match else "0"
    metrics["cpu"] = cpu_match.group(1) if cpu_match else "0"
    return metrics


def get_file_size_kb(filepath):
    if os.path.exists(filepath):
        return round(os.path.getsize(filepath) / 1024, 2)
    return 0


def log_to_csv(data, filename):
    file_exists = os.path.isfile(filename)
    with open(filename, "a", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=data.keys())
        if not file_exists:
            writer.writeheader()
        writer.writerow(data)


def cleanup_session_files():
    files = [
        "cryptocontext.bin",
        "public_key.bin",
        "secret_key.bin",
        "mult_key.bin",
        "rot_key.bin",
        "ciphertext_in.bin",
        "ciphertext_out.bin",
    ]
    for f in files:
        if os.path.exists(f):
            os.remove(f)


# ================= MQTT LOGIC =================


def on_message(client, userdata, msg):
    global state

    if not state["v1_received"]:
        v_idx = 1
        state["metrics"] = reset_metrics_dict()
        state["metrics"]["Timestamp"] = time.strftime("%H:%M:%S")
        cleanup_session_files()
    else:
        v_idx = 2

    print(
        f"\n[*] Processing Vector {v_idx} for {SELECTED_SCHEME}...", flush=True
    )

    # 1. AES Decrypt
    try:
        start_aes = time.time()
        raw_data = base64.b64decode(msg.payload)
        iv = raw_data[:16]
        ciphertext = raw_data[16:]
        cipher = AES.new(AES_KEY, AES.MODE_CBC, iv)
        decrypted = unpad(cipher.decrypt(ciphertext), AES.block_size)
        readings = [float(x) for x in decrypted.decode("utf-8").split(",")]
        data_count = len(readings)
        dynamic_csv_name = f"{SELECTED_SCHEME}-{OPERATION}-{data_count}.csv"

        state["metrics"][f"v{v_idx}_aes_dec_ms"] = round(
            (time.time() - start_aes) * 1000, 4
        )
        plaintext_kb = len(decrypted)
        state["metrics"][f"v{v_idx}_plaintext_size"] = plaintext_kb

        with open("sensor_readings.txt", "w") as f:
            for val in readings:
                f.write(f"{val}\n")
    except Exception as e:
        print(f"[!] AES Decryption failed: {e}")
        return

    # 2. FHE Encryption
    engine_path = ENGINES[SELECTED_SCHEME]
    enc_proc = subprocess.run(
        ["/usr/bin/time", "-v", engine_path], capture_output=True, text=True
    )

    enc_res = parse_time_v(enc_proc.stderr)
    state["metrics"][f"v{v_idx}_enc_kb"] = enc_res["kb"]
    state["metrics"][f"v{v_idx}_enc_cpu"] = enc_res["cpu"]

    fhe_time_match = re.search(r"Encryption Time:\s*([\d.]+)", enc_proc.stdout)
    state["metrics"][f"v{v_idx}_fhe_enc_ms"] = (
        fhe_time_match.group(1) if fhe_time_match else ""
    )
    state["metrics"][f"v{v_idx}_ciphertext_size"] = get_file_size_kb(
        "ciphertext_out.bin"
    )

    # 3. UPLOAD TO CLOUD IMMEDIATELY
    print(f"[*] Uploading Vector {v_idx} to Cloud...")
    try:
        files = {
            "context": open("cryptocontext.bin", "rb"),
            "data": open("ciphertext_out.bin", "rb"),
            "mult_key": open("mult_key.bin", "rb"),
            "rot_key": open("rot_key.bin", "rb"),
        }
        response = requests.post(CLOUD_URL, files=files)

        if response.status_code == 202:
            print(
                f"[+] Vector 1 stored on Cloud. Waiting for next MQTT message..."
            )
            state["v1_received"] = True

        elif response.status_code == 200:
            print(f"[+] Vector 2 processed. Downloading result...")
            with open("ciphertext_in.bin", "wb") as f:
                f.write(response.content)

            # 4. FHE Decryption
            dec_proc = subprocess.run(
                ["/usr/bin/time", "-v", engine_path, "--decrypt", OPERATION],
                capture_output=True,
                text=True,
            )
            dec_res = parse_time_v(dec_proc.stderr)
            state["metrics"]["fhe_dec_kb"] = dec_res["kb"]
            state["metrics"]["fhe_dec_cpu"] = dec_res["cpu"]
            dec_time_match = re.search(
                r"Decryption Time:\s*([\d.]+)", dec_proc.stdout
            )
            state["metrics"]["fhe_dec_ms"] = (
                dec_time_match.group(1) if dec_time_match else ""
            )

            cloud_time = response.headers.get("X-Compute-Time", "0")
            state["metrics"]["cloud_compute_ms"] = cloud_time

            # 5. LOG & RESET
            log_to_csv(state["metrics"], dynamic_csv_name)
            print(f"[+] Cycle complete. Metrics saved to {dynamic_csv_name}.")
            state["v1_received"] = False
        else:
            print(f"[!] Cloud Error: {response.text}")

    except Exception as e:
        print(f"[!] Cloud Request failed: {e}")


# ================= MAIN =================

cleanup_session_files()
client = mqtt.Client(
    mqtt.CallbackAPIVersion.VERSION2
)  # Fixed deprecation warning
client.on_message = on_message

print(f"--- FHE Fog Node: {SELECTED_SCHEME} Mode ---")
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.subscribe(MQTT_TOPIC)
client.loop_forever()

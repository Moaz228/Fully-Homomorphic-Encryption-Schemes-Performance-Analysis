import csv
import glob
import os
import re
import subprocess
import threading
import time

# --- OUTPUT FILE CONFIG ---
CSV_NAME = "fhe_performance_metrics.csv"


def cleanup_workspace():
    """Deletes the metrics CSV and all .bin files to ensure a fresh experiment."""
    print("🧹 Cleaning up old project files...")

    csv_files = glob.glob("*.csv") + glob.glob("cloud/*.csv")

    for file_path in csv_files:
        if file_path == CSV_NAME:
            continue
        try:
            os.remove(file_path)
            print(f"   Deleted {file_path}")
        except OSError as e:
            print(f"   Error deleting {file_path}: {e}")

    # 2. Find and remove all .bin files in root and cloud/ directories
    # Using glob allows us to find files by pattern
    bin_files = glob.glob("*.bin") + glob.glob("cloud/*.bin")

    for file_path in bin_files:
        try:
            os.remove(file_path)
            print(f"   Deleted {file_path}")
        except OSError as e:
            print(f"   Error deleting {file_path}: {e}")


def log_to_csv(data_row):
    file_exists = os.path.isfile(CSV_NAME)
    # Ensure all possible keys are present in the header
    fieldnames = ["Timestamp", "Source", "Schema", "Operation"] + list(
        patterns.keys()
    )

    with open(CSV_NAME, "a", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        if not file_exists:
            writer.writeheader()
        writer.writerow(data_row)


import csv
import os
import re
import threading
import time

# --- CONFIG ---
CSV_NAME = "fhe_performance_metrics.csv"
data_lock = threading.Lock()

# Global data store
row_data = {
    "Timestamp": "",
    "Schema": "",
    "Operation": "",
    "v1_plaintext_size": "",
    "v2_plaintext_size": "",
    "v1_ciphertext_size": "",
    "v2_ciphertext_size": "",
    "v1_aes_enc_ms": "",
    "v2_aes_enc_ms": "",
    "v1_aes_dec_ms": "",
    "v2_aes_dec_ms": "",
    "v1_enc_kb": 0.0,
    "v2_enc_kb": 0.0,
    "v1_enc_cpu": "",
    "v2_enc_cpu": "",
    "v1_fhe_enc_ms": "",
    "v2_fhe_enc_ms": "",
    "cloud_op_ms": "",
    "fhe_dec_ms": "",
    "fhe_dec_kb": "",
    "fhe_dec_cpu": "",
}

# Optimized Regex Patterns
patterns = {
    "schema": r"--- FHE Gateway: (\w+) Mode ---",
    "operation": r"operation: (\w+)",
    "v1_plaintext_size": r"First Vector Data Size = (\d+)",
    "v2_plaintext_size": r"Second Vector Data Size = (\d+)",
    "v1_aes_enc_ms": r"First Vector's AES Encryption Time: ([\d.]+) ms",
    "v2_aes_enc_ms": r"Second Vector's AES Encryption Time: ([\d.]+) ms",
    "aes_dec": r"AES Decryption Time: ([\d.]+) ms",
    "aes_delta": r"AES DRAM Delta: ([\d.]+) KB",
    "fhe_enc": r"\[\w+\] Encryption Time: ([\d.]+) ms",  # Works for any schema
    "fhe_dec": r"\[\w+\] Decryption Time: ([\d.]+) ms",  # Works for any schema
    "max_rss": r"Maximum resident set size \(kbytes\): (\d+)",
    "cpu_perc": r"Percent of CPU this job got: (\d+)%",
    "cloud_op": r"Operation Time: ([\d.]+) ms",
    "cipher_size": r"Encrypted Data Size: ([\d.]+)",
}


def monitor_output(process, name):
    """
    Monitors stdout of a process and updates the global row_data dictionary.
    Handles multi-vector logic and separates Encryption vs Decryption resources.
    """
    global row_data
    v_idx = 1
    # Initialize a state flag on the process object to track if we are decrypting
    process.is_decrypting = False

    for line in iter(process.stdout.readline, ""):
        line = line.strip()
        if not line:
            continue
        print(f"[{name}] {line}")

        with data_lock:
            # 1. State Tracking: Detect if the current command is FHE Decryption
            if "--decrypt" in line:
                process.is_decrypting = True
            elif "Command being timed" in line and "--decrypt" not in line:
                process.is_decrypting = False

            # 2. Schema and Operation Detection
            m_schema = re.search(r"FHE Gateway: (\w+) Mode", line)
            if m_schema:
                row_data["Schema"] = m_schema.group(1)

            m_op = re.search(r"operation: (\w+)", line)
            if m_op:
                row_data["Operation"] = m_op.group(1)

            # 3. Vector Index Logic
            # Switch to Vector 2 if we see Vector 2 labels or if Vector 1's AES is already done
            if "Second Vector" in line or "Loading existing keys" in line:
                v_idx = 2
            elif "[MQTT] Received data" in line and row_data["v1_aes_dec_ms"]:
                v_idx = 2

            # 4. Resource Consumption (CPU & RAM)
            # This logic prevents Decryption stats from overwriting Vector 2 stats
            m_rss = re.search(
                r"Maximum resident set size \(kbytes\): (\d+)", line
            )
            if m_rss:
                val = float(m_rss.group(1))
                if process.is_decrypting:
                    row_data["fhe_dec_kb"] = val
                else:
                    row_data[f"v{v_idx}_enc_kb"] += val

            m_cpu = re.search(r"Percent of CPU this job got: (\d+)%", line)
            if m_cpu:
                val = m_cpu.group(1)
                if process.is_decrypting:
                    row_data["fhe_dec_cpu"] = val
                else:
                    row_data[f"v{v_idx}_enc_cpu"] = val

            # 5. General Metrics (Timing and Sizes)
            # Plaintext Sizes (from Sender)
            if "Vector Data Size =" in line:
                m = re.search(r"Vector Data Size = (\d+)", line)
                if m:
                    target = (
                        "v1_plaintext_size"
                        if "First" in line
                        else "v2_plaintext_size"
                    )
                    row_data[target] = m.group(1)

            # AES Encryption Times (from Sender)
            if "AES Encryption Time:" in line:
                m = re.search(r"AES Encryption Time: ([\d.]+) ms", line)
                if m:
                    target = (
                        "v1_aes_enc_ms" if "First" in line else "v2_aes_enc_ms"
                    )
                    row_data[target] = m.group(1)

            # AES Decryption (from Fog)
            m_aes_dec = re.search(r"AES Decryption Time: ([\d.]+)", line)
            if m_aes_dec:
                row_data[f"v{v_idx}_aes_dec_ms"] = m_aes_dec.group(1)

            # AES DRAM Delta (added to memory footprint)
            m_delta = re.search(r"AES DRAM Delta: ([\d.]+) KB", line)
            if m_delta:
                row_data[f"v{v_idx}_enc_kb"] += float(m_delta.group(1))

            # FHE Timing (Encryption and Decryption)
            m_fhe_enc = re.search(r"Encryption Time: ([\d.]+) ms", line)
            if m_fhe_enc:
                row_data[f"v{v_idx}_fhe_enc_ms"] = m_fhe_enc.group(1)

            m_fhe_dec = re.search(r"Decryption Time: ([\d.]+) ms", line)
            if m_fhe_dec:
                row_data["fhe_dec_ms"] = m_fhe_dec.group(1)

            # Cloud & Network Metrics
            m_cloud = re.search(r"Operation Time: ([\d.]+) ms", line)
            if m_cloud:
                row_data["cloud_op_ms"] = m_cloud.group(1)

            m_cipher = re.search(r"Encrypted Data Size: ([\d.]+)", line)
            if m_cipher:
                row_data[f"v{v_idx}_ciphertext_size"] = m_cipher.group(1)


def log_final_row():
    """Writes the single consolidated row to CSV."""
    row_data["Timestamp"] = time.strftime("%Y-%m-%d %H:%M:%S")
    file_exists = os.path.isfile(CSV_NAME)
    with open(CSV_NAME, "a", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=row_data.keys())
        if not file_exists:
            writer.writeheader()
        writer.writerow(row_data)


def run_experiment():
    print("🚀 Starting Unified FHE Analysis...")
    cleanup_workspace()

    # 1. Start Cloud (Flask)
    cloud_proc = subprocess.Popen(
        ["python", "cloud/app.py"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    # 2. Start Fog (Gateway)
    fog_proc = subprocess.Popen(
        ["python", "gateway_mqtt.py"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    time.sleep(5)

    # 3. Start monitoring threads
    t1 = threading.Thread(target=monitor_output, args=(cloud_proc, "CLOUD"))
    t2 = threading.Thread(target=monitor_output, args=(fog_proc, "FOG"))
    # Setting as daemon ensures they exit when the main script exits
    t1.daemon = True
    t2.daemon = True
    t1.start()
    t2.start()

    # 4. Trigger the ESP (Sender)
    print("📡 Triggering ESP32 Data Flow...")
    sender_proc = subprocess.Popen(
        ["python", "test_sender.py"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    # Create a third monitoring thread for the sender
    t3 = threading.Thread(target=monitor_output, args=(sender_proc, "SENDER"))
    t3.daemon = True
    t3.start()

    print(
        "📊 Metrics are being collected. Press Ctrl+C once decryption is finished to save."
    )

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n💾 Saving final metrics to CSV...")
        # CRITICAL: Call the save function here!
        log_final_row()

        print("Stopping processes...")
        cloud_proc.terminate()
        fog_proc.terminate()
        print("✅ Done.")


if __name__ == "__main__":
    run_experiment()

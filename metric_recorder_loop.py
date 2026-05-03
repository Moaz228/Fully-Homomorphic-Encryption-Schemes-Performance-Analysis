import csv
import glob
import os
import re
import subprocess
import threading
import time

# --- CONFIG ---
CSV_NAME = "fhe_performance_metrics.csv"
data_lock = threading.Lock()


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


def reset_row_data():
    return {
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


row_data = reset_row_data()


def monitor_output(process, name):
    global row_data
    # local_v_idx tracks the context (Vector 1 vs Vector 2) for this specific thread
    local_v_idx = 1
    process.is_decrypting = False

    while True:
        line = process.stdout.readline()
        if not line:
            break
        line = line.strip()
        if not line:
            continue

        # Log to console so you can see real-time progress
        print(f"[{name}] {line}")

        with data_lock:
            # 1. CONTEXT TRACKING
            # Update the vector index based on breadcrumbs in the log
            if any(x in line for x in ["First Vector", "v1"]):
                local_v_idx = 1
            elif any(
                x in line
                for x in [
                    "Second Vector",
                    "Loading existing keys",
                    "vector2",
                    "v2",
                ]
            ):
                local_v_idx = 2

            # Detect if we are in the final FHE Decryption phase (Resource tracking)
            if "--decrypt" in line:
                process.is_decrypting = True
            elif "Command being timed" in line and "--decrypt" not in line:
                process.is_decrypting = False

            # 2. TIMING CAPTURE (AES, FHE, and Cloud)
            m_ms = re.search(r"Time: ([\d.]+) ms", line)
            if m_ms:
                val = m_ms.group(1)

                if "AES Decryption Time:" in line:
                    # Fill v1 slot first, then v2, unless the line explicitly says otherwise
                    if "First" in line:
                        row_data["v1_aes_dec_ms"] = val
                    elif "Second" in line:
                        row_data["v2_aes_dec_ms"] = val
                    elif not row_data["v1_aes_dec_ms"]:
                        row_data["v1_aes_dec_ms"] = val
                    else:
                        row_data["v2_aes_dec_ms"] = val

                elif "AES Encryption Time:" in line:
                    if "First" in line:
                        row_data["v1_aes_enc_ms"] = val
                    elif "Second" in line:
                        row_data["v2_aes_enc_ms"] = val
                    elif not row_data["v1_aes_enc_ms"]:
                        row_data["v1_aes_enc_ms"] = val
                    else:
                        row_data["v2_aes_enc_ms"] = val

                elif "Operation Time:" in line:
                    # Capture the Cloud Processing time
                    row_data["cloud_op_ms"] = val

                elif "Encryption Time:" in line:
                    # FHE Encryption for the current vector context
                    row_data[f"v{local_v_idx}_fhe_enc_ms"] = val

                elif "Decryption Time:" in line:
                    # Final FHE Decryption result
                    row_data["fhe_dec_ms"] = val

            # 3. SIZE CAPTURE (Plaintext & Ciphertext)
            # Plaintext Format: "Vector Data Size = 1024"
            if "Vector Data Size" in line:
                m_plain = re.search(r"Size\s*=\s*(\d+)", line)
                if m_plain:
                    target = (
                        "v1_plaintext_size"
                        if "First" in line
                        else "v2_plaintext_size"
                    )
                    row_data[target] = m_plain.group(1)

            # Ciphertext Format: "Encrypted Data Size: 5400.5"
            if "Encrypted Data Size" in line:
                m_cipher = re.search(r"Size:\s*([\d.]+)", line)
                if m_cipher:
                    row_data[f"v{local_v_idx}_ciphertext_size"] = (
                        m_cipher.group(1)
                    )

            # 4. RESOURCE CAPTURE (RAM & CPU)
            if "Maximum resident set size" in line:
                m_rss = re.search(r"size \(kbytes\): (\d+)", line)
                if m_rss:
                    val = float(m_rss.group(1))
                    if process.is_decrypting:
                        row_data["fhe_dec_kb"] = val
                    else:
                        row_data[f"v{local_v_idx}_enc_kb"] += val

            if "Percent of CPU" in line:
                m_cpu = re.search(r"got: (\d+)%", line)
                if m_cpu:
                    val = m_cpu.group(1)
                    if process.is_decrypting:
                        row_data["fhe_dec_cpu"] = val
                    else:
                        row_data[f"v{local_v_idx}_enc_cpu"] = val


def run_single_iteration(schema, operation):
    cleanup_workspace()
    global row_data
    row_data = reset_row_data()

    # Pre-set Schema/Op so they aren't empty if logs miss them
    row_data["Schema"] = schema
    row_data["Operation"] = operation

    cloud_proc = subprocess.Popen(
        ["python", "cloud/app.py"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    env_vars = os.environ.copy()
    env_vars["SELECTED_SCHEME"] = schema
    env_vars["SELECTED_OPERATION"] = operation

    fog_proc = subprocess.Popen(
        ["python", "gateway_mqtt.py"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
        env=env_vars,  # <--- Add this
    )

    time.sleep(3)  # Wait for servers to stabilize

    threading.Thread(
        target=monitor_output, args=(cloud_proc, "CLOUD"), daemon=True
    ).start()
    threading.Thread(
        target=monitor_output, args=(fog_proc, "FOG"), daemon=True
    ).start()

    sender_proc = subprocess.Popen(
        ["python", "test_sender.py"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    threading.Thread(
        target=monitor_output, args=(sender_proc, "SENDER"), daemon=True
    ).start()

    # 🚦 CRITICAL WAIT LOGIC
    print("⏳ Waiting for cycle to complete...")
    timeout = 90
    start_time = time.time()

    # Wait until Decryption Time is recorded AND we have the RSS value for it
    while not (row_data["fhe_dec_ms"] and row_data["fhe_dec_kb"]):
        if time.time() - start_time > timeout:
            print("⚠️ Iteration Timed Out.")
            break
        time.sleep(0.5)

    time.sleep(2)  # Final grace period for remaining CPU logs to arrive

    with data_lock:
        row_data["Timestamp"] = time.strftime("%H:%M:%S")
        file_exists = os.path.isfile(CSV_NAME)
        with open(CSV_NAME, "a", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=row_data.keys())
            if not file_exists:
                writer.writeheader()
            writer.writerow(row_data)

    print("💾 Metrics saved. Cleaning up processes...")
    for p in [cloud_proc, fog_proc, sender_proc]:
        try:
            p.terminate()
            p.wait(timeout=5)
        except:
            p.kill()


def main():
    # 1. Map for Schemas
    schema_map = {"1": "BGV", "2": "BFV", "3": "CKKS"}
    print("\nSelect Schema:\n1. BGV\n2. BFV\n3. CKKS")
    schema_choice = input("Choice (1-3): ").strip()
    schema = schema_map.get(schema_choice, "BGV")

    # 2. Map for Operations
    op_map = {"1": "add", "2": "multiply", "3": "average"}
    print("\nSelect Operation:\n1. Add\n2. Multiply\n3. Average")
    op_choice = input("Choice (1-3): ").strip()
    operation = op_map.get(op_choice, "add")

    iters = int(input("\nIterations: "))

    # Pass the actual strings ("BGV", "add") to the runner
    for i in range(iters):
        print(
            f"\n--- RUN {i+1}/{iters} [Scheme: {schema}, Op: {operation}] ---"
        )
        run_single_iteration(schema, operation)
        time.sleep(2)


if __name__ == "__main__":
    main()

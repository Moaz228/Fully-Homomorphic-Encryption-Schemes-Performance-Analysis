import csv
import os
import re
import subprocess
import threading
import time

# --- CONFIG ---
# Update this to your Pi's IP to check if it's reachable before starting
FOG_IP = "192.168.1.XX"


def reset_row_data():
    return {
        "Timestamp": "",
        "v1_plaintext_size": "",
        "v2_plaintext_size": "",
        "v1_aes_enc_ms": "",
        "v2_aes_enc_ms": "",
        "sender_status": "Success",
    }


row_data = reset_row_data()
data_lock = threading.Lock()


def monitor_sender(process):
    global row_data
    v_idx = 1
    while True:
        line = process.stdout.readline()
        if not line:
            break
        line = line.strip()
        print(f"[SENDER] {line}")

        with data_lock:
            # Capture Plaintext Sizes
            if "Vector Data Size" in line:
                m = re.search(r"Size\s*=\s*(\d+)", line)
                if m:
                    target = (
                        "v1_plaintext_size"
                        if v_idx == 1
                        else "v2_plaintext_size"
                    )
                    row_data[target] = m.group(1)
                    v_idx = 2

            # Capture AES Encryption Times
            if "AES Encryption Time:" in line:
                m = re.search(r"Time: ([\d.]+) ms", line)
                if m:
                    target = (
                        "v1_aes_enc_ms"
                        if "First" in line or not row_data["v1_aes_enc_ms"]
                        else "v2_aes_enc_ms"
                    )
                    row_data[target] = m.group(1)


def run_iteration(csv_filename):
    global row_data
    row_data = reset_row_data()
    row_data["Timestamp"] = time.strftime("%H:%M:%S")

    # We only start the SENDER because Cloud and Fog are already running
    sender_proc = subprocess.Popen(
        ["python", "test_sender.py"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    monitor_thread = threading.Thread(
        target=monitor_sender, args=(sender_proc,)
    )
    monitor_thread.start()

    # Wait for sender to finish its work
    sender_proc.wait()
    monitor_thread.join()

    # Save Sender-side metrics
    file_exists = os.path.isfile(csv_filename)
    with open(csv_filename, "a", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=row_data.keys())
        if not file_exists:
            writer.writeheader()
        writer.writerow(row_data)


def main():
    print("🚀 Remote Test Manager (Workstation Side)")
    iters = int(input("How many iterations to run? "))

    filename = f"sender_metrics_{time.strftime('%Y%m%d_%H%M%S')}.csv"

    for i in range(iters):
        print(f"\n--- TRIGGERING RUN {i+1}/{iters} ---")
        run_iteration(filename)
        # Wait for the Fog (Pi) to finish its FHE/Cloud cycle before sending next
        print("⏳ Waiting 5s for Fog to complete its cycle and log data...")
        time.sleep(5)

    print(f"\n✅ Testing Complete. Sender metrics saved to {filename}")
    print(
        "🔗 Remember to collect 'fog_performance_log.csv' from the Raspberry Pi."
    )


if __name__ == "__main__":
    main()

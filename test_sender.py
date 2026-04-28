import base64
import random
import time

import paho.mqtt.publish as publish
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad

# 100 250 500
# This simulates the ESP32's job
KEY = b"my_super_secret1"
# first_vector = [str(round(random.uniform(20.0, 50.0), 3)) for _ in range(500)]
# second_vector = [str(round(random.uniform(20.0, 50.0), 3)) for _ in range(500)]
# 1. Generate the same vectors
first_vector = [round(random.uniform(20.0, 50.0), 3) for _ in range(500)]
second_vector = [round(random.uniform(20.0, 50.0), 3) for _ in range(500)]
# random_values = [str(random.randint(20, 50)) for _ in range(60)]

DATA1 = ",".join(map(str, first_vector)).encode("utf-8")
print(f"First Vector Data Size = {len(DATA1)}")

DATA2 = ",".join(map(str, second_vector)).encode("utf-8")
print(f"Second Vector Data Size = {len(DATA2)}")

print("\n--- SENSOR DATA PREVIEW (First 10) ---")
print(f"Vector 1: {first_vector[:10]}")
print(f"Vector 2: {second_vector[:10]}")
print("---------------------------------------\n")
# Encrypt like the ESP32 would
start_time = time.time()
cipher = AES.new(KEY, AES.MODE_CBC)
iv = cipher.iv
ciphertext1 = cipher.encrypt(pad(DATA1, AES.block_size))
end_time = time.time()
print(
    f"First Vector's AES Encryption Time: {(end_time - start_time)*1000:.4f} ms"
)
start_time = time.time()
cipher2 = AES.new(KEY, AES.MODE_CBC)
iv2 = cipher2.iv
ciphertext2 = cipher2.encrypt(pad(DATA2, AES.block_size))
end_time = time.time()
print(
    f"Second Vector's AES Encryption Time: {(end_time - start_time)*1000:.4f} ms"
)


# ================= TRUTH CALCULATION =================
# This is what the FHE result SHOULD be.
# 1. Temporal Average (per index)
true_temporal_avgs = [
    (first_vector[i] + second_vector[i]) / 2.0 for i in range(500)
]

# 2. Global Spatial Average (The final single number)
true_global_avg = sum(true_temporal_avgs) / len(true_temporal_avgs)

print("\n" + "=" * 40)
print("EXPECTED RESULTS (GROUND TRUTH)")
print("=" * 40)
print(f"First 5 Temporal Averages (V1+V2)/2:")
for i in range(5):
    print(f"  Index {i}: {true_temporal_avgs[i]:.3f}")

print(f"\nFINAL GLOBAL SPATIAL AVERAGE: {true_global_avg:.5f}")
print("=" * 40 + "\n")
payload1 = base64.b64encode(iv + ciphertext1)

payload2 = base64.b64encode(iv2 + ciphertext2)
publish.single(
    "grad_project/sensor_data", payload1, hostname="broker.hivemq.com"
)
time.sleep(1)
publish.single(
    "grad_project/sensor_data", payload2, hostname="broker.hivemq.com"
)

print("Test packet sent to MQTT!")

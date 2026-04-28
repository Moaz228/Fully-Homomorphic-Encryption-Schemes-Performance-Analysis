import base64
import random
import time

import paho.mqtt.publish as publish
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad

# 100 250 500
# This simulates the ESP32's job
KEY = b"my_super_secret1"
first_vector = [str(round(random.uniform(20.0, 50.0), 3)) for _ in range(500)]
second_vector = [str(round(random.uniform(20.0, 50.0), 3)) for _ in range(500)]

# random_values = [str(random.randint(20, 50)) for _ in range(60)]

DATA1 = ",".join(first_vector).encode("utf-8")
print(len(DATA1))

DATA2 = ",".join(second_vector).encode("utf-8")
print(len(DATA2))
# Encrypt like the ESP32 would
start_time = time.time()
cipher = AES.new(KEY, AES.MODE_CBC)
iv = cipher.iv
ciphertext1 = cipher.encrypt(pad(DATA1, AES.block_size))
cipher2 = AES.new(KEY, AES.MODE_CBC)
iv2 = cipher2.iv
ciphertext2 = cipher2.encrypt(pad(DATA2, AES.block_size))
end_time = time.time()
payload1 = base64.b64encode(iv + ciphertext1)

payload2 = base64.b64encode(iv2 + ciphertext2)
print(f"AES Encryption Time: {(end_time - start_time)*1000:.4f} ms")
publish.single(
    "grad_project/sensor_data", payload1, hostname="broker.hivemq.com"
)
time.sleep(1)
publish.single(
    "grad_project/sensor_data", payload2, hostname="broker.hivemq.com"
)

print("Test packet sent to MQTT!")

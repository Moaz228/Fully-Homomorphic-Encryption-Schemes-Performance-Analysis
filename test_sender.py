import base64
import random
import time

import paho.mqtt.publish as publish
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad

# 100 250 500
# This simulates the ESP32's job
KEY = b"my_super_secret1"
random_values = [str(round(random.uniform(20.0, 50.0), 3)) for _ in range(60)]

# random_values = [str(random.randint(20, 50)) for _ in range(60)]

DATA = ",".join(random_values).encode("utf-8")
print(len(DATA))

# Encrypt like the ESP32 would
start_time = time.time()
cipher = AES.new(KEY, AES.MODE_CBC)
iv = cipher.iv
ciphertext = cipher.encrypt(pad(DATA, AES.block_size))
end_time = time.time()
payload = base64.b64encode(iv + ciphertext)

print(f"AES Encryption Time: {(end_time - start_time)*1000:.4f} ms")
publish.single(
    "grad_project/sensor_data", payload, hostname="broker.hivemq.com"
)
print("Test packet sent to MQTT!")

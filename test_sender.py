import paho.mqtt.publish as publish
import base64
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad

# This simulates the ESP32's job
KEY = b'my_super_secret1'
DATA = ",".join(["2543"] * 100).encode('utf-8')

# Encrypt like the ESP32 would
cipher = AES.new(KEY, AES.MODE_CBC)
iv = cipher.iv
ciphertext = cipher.encrypt(pad(DATA, AES.block_size))
payload = base64.b64encode(iv + ciphertext)

publish.single("grad_project/sensor_data", payload, hostname="broker.hivemq.com")
print("Test packet sent to MQTT!")
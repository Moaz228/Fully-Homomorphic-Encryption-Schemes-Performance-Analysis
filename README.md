# Secure IoT Edge Gateway: FHE Performance Analysis

This project implements a secure IoT Gateway designed to bridge an ESP32 (Edge) and a Cloud Platform. It uses a dual-layer security approach: **AES-128-CBC** for local network transit and **Fully Homomorphic Encryption (FHE)** for private cloud processing.

## 🚀 Key Features
* **Multi-Scheme Support:** Implements BFV, BGV, and CKKS schemes using OpenFHE.
* **Hybrid Decryption:** Python-based AES decryption followed by C++ FHE encryption.
* **MQTT Integration:** Live listening for encrypted sensor data.
* **Performance Sweeper:** Automated benchmarking tool for scheme comparison.

## 📁 Project Structure
* `/src`: C++ source files for BFV, BGV, and CKKS engines.
* `/include`: Header files (`EncDataContainer.hxx`) for serialization.
* `/plot`: Data visualizations and performance graphs.
* `gateway_mqtt.py`: The main Gateway controller.
* `performance_sweeper.py`: The benchmarking suite.

## 🛠️ Installation & Setup
1. **Dependencies:**
   - OpenFHE (C++)
   - Python 3.x (`pip install pycryptodome paho-mqtt`)
2. **Build:**
   ```bash
   mkdir build && cd build
   cmake ..
   make
   Run: python3 gateway_mqtt.py
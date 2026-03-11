```markdown
# Performance Analysis Report: FHE for IoT Telemetry

## 1. Objective
To analyze the trade-offs between BFV, BGV, and CKKS encryption schemes when running on a resource-constrained Edge Gateway (Raspberry Pi architecture).

## 2. Experimental Setup
- **Hardware:** Raspberry Pi / WSL2 Ubuntu.
- **Payload:** 100 simulated sensor readings (float precision scaled to integers).
- **Library:** OpenFHE v1.2.0.

## 3. Results (Summary Table)
| Scheme | Encrypt (ms) | Decrypt (ms) | File Size | Best For |
| :--- | :--- | :--- | :--- | :--- |
| **BFV** | 62.66 | 10.42 | ~385 KB | Low Bandwidth |
| **BGV** | 26.38 | 19.95 | ~1.1 MB | High Speed |
| **CKKS** | 20.18 | 26.33 | ~769 KB | Data Science/AI |

## 4. Conclusion
For this IoT gateway use case, **CKKS** is the recommended scheme due to its superior encryption speed (20ms) and native support for approximate floating-point arithmetic, which is ideal for sensor telemetry.
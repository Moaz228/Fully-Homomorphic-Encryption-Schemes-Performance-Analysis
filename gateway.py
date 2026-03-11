import subprocess
import os

# The path to your compiled C++ engine
FHE_ENGINE = "./build/bin.out"

def run_gateway_pipeline(simulated_readings):
    # 1. Write the decrypted readings to the text file
    # We save it where the C++ engine can see it
    with open("sensor_readings.txt", "w") as f:
        for val in simulated_readings:
            f.write(f"{val}\n")
    
    print("[Python] AES Decryption Simulated. Data saved to sensor_readings.txt.")
    print("[Python] Triggering C++ BGV Engine...")

    # 2. Trigger the C++ Engine
    # We run it and capture the output so we can see it in the Python terminal
    result = subprocess.run([FHE_ENGINE], capture_output=True, text=True)
    
    if result.returncode == 0:
        print("\n--- C++ OUTPUT ---")
        print(result.stdout)
        print("------------------")
    else:
        print(f"[!] C++ Engine Error: {result.stderr}")

if __name__ == "__main__":
    print("==========================================")
    print("   RASPBERRY PI GATEWAY CONTROLLER       ")
    print("==========================================\n")
    
    # Simulate receiving 100 sensor readings (e.g., 25.43 scaled to 2543)
    # In the future, your AES decryption function will produce this list
    test_data = [2543] * 100 
    
    run_gateway_pipeline(test_data)
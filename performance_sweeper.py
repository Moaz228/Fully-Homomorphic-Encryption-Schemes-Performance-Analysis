import subprocess
import os
import time

# Configuration: Map the name to the compiled binary path
ENGINES = {
    "BFV": "./build/bin.out",
    "BGV": "./build/bgv_bin.out",
    "CKKS": "./build/ckks_bin.out"
}

def run_benchmark():
    # 1. Prepare the exact same data for all tests
    # 100 sensor readings (25.43 * 100)
    test_data = [2543] * 100
    with open("sensor_readings.txt", "w") as f:
        for val in test_data:
            f.write(f"{val}\n")

    results = []

    print("=====================================================")
    print("   FHE SCHEME PERFORMANCE SWEEPER (RPI GATEWAY)      ")
    print("=====================================================\n")

    for name, path in ENGINES.items():
        if not os.path.exists(path):
            print(f"[!] Warning: {name} binary not found at {path}. Skipping...")
            continue

        print(f"[*] Benchmarking {name}...")
        
        # Run the C++ engine and capture output
        start_wall_time = time.time()
        process = subprocess.run([path], capture_output=True, text=True)
        end_wall_time = time.time()

        if process.returncode == 0:
            # Parse the output for the specific timing line
            # This looks for the line containing "Encryption Time"
            lines = process.stdout.split('\n')
            enc_time = "N/A"
            dec_time = "N/A"
            
            for line in lines:
                if "Encryption Time:" in line:
                    enc_time = line.split(":")[1].strip()
                if "Decryption Time:" in line:
                    dec_time = line.split(":")[1].strip()
            
            results.append({
                "Scheme": name,
                "Enc Time": enc_time,
                "Dec Time": dec_time,
                "Total (Wall)": f"{int((end_wall_time - start_wall_time)*1000)} ms"
            })
        else:
            print(f"[!] Error running {name}: {process.stderr}")

    # 2. Display Final Comparison Table
    print("\n" + "="*65)
    print(f"{'Scheme':<10} | {'Enc Time':<15} | {'Dec Time':<15} | {'Wall Clock'}")
    print("-" * 65)
    for r in results:
        print(f"{r['Scheme']:<10} | {r['Enc Time']:<15} | {r['Dec Time']:<15} | {r['Total (Wall)']}")
    print("="*65)

if __name__ == "__main__":
    run_benchmark()
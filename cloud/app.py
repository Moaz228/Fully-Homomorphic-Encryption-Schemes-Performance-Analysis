import os
import subprocess

from flask import Flask, request, send_file

app = Flask(__name__)

# Temporary storage paths
CT1_PATH = "cloud_ciphertext_1.bin"
CT2_PATH = "cloud_ciphertext_2.bin"


"""
def compute(operation):
    # Clear old keys to prevent scheme mismatch errors
    for f in ["mult_key.bin", "rot_key.bin"]:
        if os.path.exists(f):
            os.remove(f)

    # Save incoming data
    request.files["context"].save("cloud_context.bin")
    request.files["data"].save("cloud_ciphertext.bin")

    # Save keys if provided by the gateway
    if "mult_key" in request.files:
        request.files["mult_key"].save("mult_key.bin")
    if "rot_key" in request.files:
        request.files["rot_key"].save("rot_key.bin")

    # Run the universal cloud engine
    subprocess.run(["../build/cloud_math.out", operation], check=True)

    return send_file("cloud_result.bin")
"""


@app.route("/compute/<operation>", methods=["POST"])
def compute(operation):
    # 1. Save Keys and Context (usually same for both vectors)
    request.files["context"].save("cloud_context.bin")
    if "mult_key" in request.files:
        request.files["mult_key"].save("mult_key.bin")
    if "rot_key" in request.files:
        request.files["rot_key"].save("rot_key.bin")

    # 2. Logic: Check if this is the first or second vector
    if not os.path.exists(CT1_PATH):
        # This is the first vector
        request.files["data"].save(CT1_PATH)
        return "Vector 1 received, waiting for Vector 2", 202
    else:
        # This is the second vector
        request.files["data"].save(CT2_PATH)

        print(f"[*] Both vectors received. Running {operation}...")

        # 3. Run the C++ engine (Updated to expect two file inputs)
        # Note: You'll need to update your C++ 'cloud_math.cpp' to load two ciphertexts
        try:
            subprocess.run(
                ["../build/cloud_math.out", operation, CT1_PATH, CT2_PATH],
                check=True,
            )

            # 4. Cleanup temporary files after computation
            os.remove(CT1_PATH)
            os.remove(CT2_PATH)

            print("Clearing old Data...")
            for f in [
                "cryptocontext.bin",
                "cloud_context.bin",
                "public_key.bin",
                "secret_key.bin",
                "mult_key.bin",
                "rot_key.bin",
                "ciphertext_in.bin",
                "ciphertext_out.bin",
            ]:
                if os.path.exists(f):
                    os.remove(f)

            return send_file("cloud_result.bin")

        except Exception as e:
            return f"Computation failed: {str(e)}", 500


if __name__ == "__main__":
    app.run(port=5000)

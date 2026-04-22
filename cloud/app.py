import os
import subprocess

from flask import Flask, request, send_file

app = Flask(__name__)


@app.route("/compute/<operation>", methods=["POST"])
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


if __name__ == "__main__":
    app.run(port=5000)

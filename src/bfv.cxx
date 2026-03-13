#include "openfhe.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

// Serialization Headers
#include "ciphertext-ser.h"
#include "cryptocontext-ser.h"
#include "key/key-ser.h"
#include "scheme/bfvrns/bfvrns-ser.h"

using namespace lbcrypto;

#define start_time(name)                                                       \
  auto name##_start = std::chrono::high_resolution_clock::now()
#define end_time(name)                                                         \
  auto name##_end = std::chrono::high_resolution_clock::now()
#define time_duration_ms(name)                                                 \
  (std::chrono::duration_cast<std::chrono::nanoseconds>(name##_end -           \
                                                        name##_start)          \
       .count() /                                                              \
   1000000.0)

int main(int argc, char *argv[]) {
  bool decryptMode = (argc > 1 && std::string(argv[1]) == "--decrypt");

  if (!decryptMode) {
    // --- PHASE 1: ENCRYPTION ---
    CCParams<CryptoContextBFVRNS> params;
    params.SetPlaintextModulus(786433);
    params.SetMultiplicativeDepth(2);

    CryptoContext<DCRTPoly> cc = GenCryptoContext(params);
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(ADVANCEDSHE); // Added for EvalSum

    KeyPair<DCRTPoly> keyPair = cc->KeyGen();

    // --- KEY GENERATION (REQUIRED FOR CLOUD) ---
    cc->EvalMultKeyGen(keyPair.secretKey);
    cc->EvalSumKeyGen(keyPair.secretKey); // Required for Average/Rotation

    // Load data
    std::vector<int64_t> gateway_input;
    std::ifstream inputFile("sensor_readings.txt");
    int64_t temp_val;
    while (inputFile >> temp_val)
      gateway_input.push_back(temp_val);
    if (gateway_input.empty())
      gateway_input.assign(100, 2543);

    // Encrypt
    Plaintext pt = cc->MakePackedPlaintext(gateway_input);
    start_time(enc);
    auto ct = cc->Encrypt(keyPair.publicKey, pt);
    end_time(enc);

    // --- UPDATED EXPORT SECTION ---
    Serial::SerializeToFile("cryptocontext.bin", cc, SerType::BINARY);
    Serial::SerializeToFile("public_key.bin", keyPair.publicKey,
                            SerType::BINARY);
    Serial::SerializeToFile("secret_key.bin", keyPair.secretKey,
                            SerType::BINARY);
    Serial::SerializeToFile("ciphertext_out.bin", ct, SerType::BINARY);

    // Export Multiplication Key
    std::ofstream multKeyFile("mult_key.bin", std::ios::binary);
    cc->SerializeEvalMultKey(multKeyFile, SerType::BINARY);

    // Export Rotation (Sum) Key
    std::ofstream rotKeyFile("rot_key.bin", std::ios::binary);
    cc->SerializeEvalAutomorphismKey(rotKeyFile, SerType::BINARY);

    std::cout << "[C++] Encrypted and Serialized (Keys included). Time: "
              << time_duration_ms(enc) << " ms" << std::endl;

  } else {
    // --- PHASE 2: DECRYPTION ---
    CryptoContext<DCRTPoly> cc;
    PrivateKey<DCRTPoly> sk;
    Ciphertext<DCRTPoly> ct;

    if (!Serial::DeserializeFromFile("cryptocontext.bin", cc, SerType::BINARY))
      return 1;
    if (!Serial::DeserializeFromFile("secret_key.bin", sk, SerType::BINARY))
      return 1;
    if (!Serial::DeserializeFromFile("ciphertext_in.bin", ct,
                                     SerType::BINARY)) {
      std::cout << "[C++] Error: ciphertext_in.bin not found!" << std::endl;
      return 1;
    }

    Plaintext result;
    start_time(dec);
    cc->Decrypt(sk, ct, &result);
    end_time(dec);

    result->SetLength(100);

    // Note: Since this is BFV (Integers), we sum the values.
    // If the Cloud did 'EvalSum', the result is in result->GetPackedValue()[0]
    int64_t sum_val = result->GetPackedValue()[0];

    std::cout << "[C++] Decrypted! Time: " << time_duration_ms(dec) << " ms"
              << std::endl;
    std::cout << "[+] Final Sum: " << sum_val << std::endl;
    std::cout << "[+] Calculated Average: " << (double)sum_val / 100.0
              << std::endl;
  }
  return 0;
}

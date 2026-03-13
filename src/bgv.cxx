#include "openfhe.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

// Serialization Headers
#include "ciphertext-ser.h"
#include "cryptocontext-ser.h"
#include "key/key-ser.h"
#include "scheme/bgvrns/bgvrns-ser.h"

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
    CCParams<CryptoContextBGVRNS> params;
    params.SetPlaintextModulus(786433); // High modulus for 100x sums
    params.SetMultiplicativeDepth(2);

    CryptoContext<DCRTPoly> cc = GenCryptoContext(params);
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(ADVANCEDSHE); // Required for EvalSum

    KeyPair<DCRTPoly> keyPair = cc->KeyGen();
    cc->EvalMultKeyGen(keyPair.secretKey);
    cc->EvalSumKeyGen(keyPair.secretKey);

    // Load data from sensor_readings.txt
    std::vector<int64_t> gateway_input;
    std::ifstream inputFile("sensor_readings.txt");
    int64_t temp_val;
    while (inputFile >> temp_val)
      gateway_input.push_back(temp_val);
    if (gateway_input.empty())
      gateway_input.assign(100, 2543);

    Plaintext pt = cc->MakePackedPlaintext(gateway_input);
    start_time(enc);
    auto ct = cc->Encrypt(keyPair.publicKey, pt);
    end_time(enc);

    // Export for Cloud
    Serial::SerializeToFile("cryptocontext.bin", cc, SerType::BINARY);
    Serial::SerializeToFile("public_key.bin", keyPair.publicKey,
                            SerType::BINARY);
    Serial::SerializeToFile("secret_key.bin", keyPair.secretKey,
                            SerType::BINARY);
    Serial::SerializeToFile("ciphertext_out.bin", ct, SerType::BINARY);

    std::ofstream multKeyFile("mult_key.bin", std::ios::binary);
    cc->SerializeEvalMultKey(multKeyFile, SerType::BINARY);
    std::ofstream rotKeyFile("rot_key.bin", std::ios::binary);
    cc->SerializeEvalAutomorphismKey(rotKeyFile, SerType::BINARY);

    std::cout << "[BGV] Encrypted & Keys Serialized. Time: "
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
    if (!Serial::DeserializeFromFile("ciphertext_in.bin", ct, SerType::BINARY))
      return 1;

    Plaintext result;
    cc->Decrypt(sk, ct, &result);
    result->SetLength(100);

    int64_t val = result->GetPackedValue()[0];
    std::cout << "[BGV] Decrypted Sum: " << val << " | Avg: " << val / 100.0
              << std::endl;
  }
  return 0;
}

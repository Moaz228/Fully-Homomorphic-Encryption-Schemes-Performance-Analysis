#include "constants-defs.h"
#include "openfhe.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

// Serialization Headers
#include "ciphertext-ser.h"
#include "cryptocontext-ser.h"
#include "key/key-ser.h"
#include "scheme/ckksrns/ckksrns-ser.h"

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
    CCParams<CryptoContextCKKSRNS> params;
    params.SetMultiplicativeDepth(2);
    params.SetScalingModSize(50);
    params.SetBatchSize(64); // Power of 2 for CKKS
    params.SetScalingTechnique(lbcrypto::FLEXIBLEAUTO);

    CryptoContext<DCRTPoly> cc = GenCryptoContext(params);
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(ADVANCEDSHE);

    KeyPair<DCRTPoly> keyPair = cc->KeyGen();
    cc->EvalMultKeyGen(keyPair.secretKey);
    cc->EvalSumKeyGen(keyPair.secretKey);

    // Load and convert to double
    std::vector<double> gateway_input;
    std::ifstream inputFile("sensor_readings.txt");
    double temp_val;
    while (inputFile >> temp_val)
      gateway_input.push_back(temp_val);
    if (gateway_input.empty())
      gateway_input.assign(100, 25.43);

    gateway_input.resize(64, 0.0);

    Plaintext pt = cc->MakeCKKSPackedPlaintext(gateway_input);
    start_time(enc);
    auto ct = cc->Encrypt(keyPair.publicKey, pt);
    end_time(enc);

    // Export
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

    std::cout << "[CKKS] Encrypted & Keys Serialized. Time: "
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

    // CKKS uses GetRealPackedValue
    auto values = result->GetRealPackedValue();
    std::cout << "[CKKS] Decrypted Result: " << values[0] << std::endl;
  }
  return 0;
}

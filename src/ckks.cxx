#include "constants-defs.h"
#include "openfhe.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
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

inline bool fileExists(const std::string &name) {
  struct stat buffer;
  return (stat(name.c_str(), &buffer) == 0);
}

int main(int argc, char *argv[]) {
  bool decryptMode = (argc > 1 && std::string(argv[1]) == "--decrypt");

  if (!decryptMode) {
    CryptoContext<DCRTPoly> cc;
    PublicKey<DCRTPoly> pk;
    PrivateKey<DCRTPoly> sk;
    // --- STEP 1: KEY PERSISTENCE LOGIC ---
    if (fileExists("cryptocontext.bin") && fileExists("public_key.bin")) {
      // Re-use existing keys so Vector 1 and Vector 2 match
      std::cout << "[CKKS] Found existing keys. Loading..." << std::endl;
      Serial::DeserializeFromFile("cryptocontext.bin", cc, SerType::BINARY);
      Serial::DeserializeFromFile("public_key.bin", pk, SerType::BINARY);
    } else {
      // Generate brand new keys (only happens for the very first vector)
      std::cout << "[CKKS] No keys found. Generating new context..."
                << std::endl;
      CCParams<CryptoContextCKKSRNS> params;
      params.SetMultiplicativeDepth(2);
      params.SetScalingModSize(50);
      params.SetBatchSize(512);
      params.SetScalingTechnique(lbcrypto::FLEXIBLEAUTO);

      cc = GenCryptoContext(params);
      cc->Enable(PKE);
      cc->Enable(KEYSWITCH);
      cc->Enable(LEVELEDSHE);
      cc->Enable(ADVANCEDSHE);

      KeyPair<DCRTPoly> keyPair = cc->KeyGen();
      pk = keyPair.publicKey;
      sk = keyPair.secretKey;

      cc->EvalMultKeyGen(sk);
      cc->EvalSumKeyGen(sk);

      // Save the context and keys for the second vector to use
      Serial::SerializeToFile("cryptocontext.bin", cc, SerType::BINARY);
      Serial::SerializeToFile("public_key.bin", pk, SerType::BINARY);
      Serial::SerializeToFile("secret_key.bin", sk, SerType::BINARY);

      std::ofstream multKeyFile("mult_key.bin", std::ios::binary);
      cc->SerializeEvalMultKey(multKeyFile, SerType::BINARY);
      std::ofstream rotKeyFile("rot_key.bin", std::ios::binary);
      cc->SerializeEvalAutomorphismKey(rotKeyFile, SerType::BINARY);
    }

    /*  // --- PHASE 1: ENCRYPTION ---
      CCParams<CryptoContextCKKSRNS> params;
      params.SetMultiplicativeDepth(2);
      params.SetScalingModSize(50);
      params.SetBatchSize(512); // Power of 2 for CKKS
      params.SetScalingTechnique(lbcrypto::FLEXIBLEAUTO);

      CryptoContext<DCRTPoly> cc = GenCryptoContext(params);
      cc->Enable(PKE);
      cc->Enable(KEYSWITCH);
      cc->Enable(LEVELEDSHE);
      cc->Enable(ADVANCEDSHE);

      KeyPair<DCRTPoly> keyPair = cc->KeyGen();
      cc->EvalMultKeyGen(keyPair.secretKey);
      cc->EvalSumKeyGen(keyPair.secretKey);
      */

    // Load and convert to double
    std::vector<double> gateway_input;
    std::ifstream inputFile("sensor_readings.txt");
    double temp_val;
    while (inputFile >> temp_val)
      gateway_input.push_back(temp_val);
    if (gateway_input.empty())
      gateway_input.assign(500, 0.0);

    gateway_input.resize(512, 0.0);

    Plaintext pt = cc->MakeCKKSPackedPlaintext(gateway_input);
    start_time(enc);
    auto ct = cc->Encrypt(pk, pt);
    end_time(enc);

    Serial::SerializeToFile("ciphertext_out.bin", ct, SerType::BINARY);

    /*
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
*/
    std::cout << "[CKKS] Encryption Time: " << time_duration_ms(enc) << " ms"
              << std::endl;

  } else {
    // --- PHASE 2: DECRYPTION ---
    //
    //
    std::string op = (argc > 2) ? argv[2] : "add";
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
    start_time(dec);
    cc->Decrypt(sk, ct, &result);
    end_time(dec);
    size_t vectorSize = 500;
    result->SetLength(vectorSize);

    std::cout << "[CKKS] Decryption Time: " << time_duration_ms(dec) << " ms"
              << std::endl;

    // CKKS uses GetRealPackedValue
    auto values = result->GetRealPackedValue();
    // std::cout << "[CKKS] Decrypted Result: " << values[0] << std::endl;
    if (op == "average") {
      start_time(avg_op);
      double totalSum = 0.0;

      for (size_t i = 0; i < vectorSize; i++) {
        // First, get the average of the two vectors for this sensor
        double sensorAvg = values[i] / 2.0;
        totalSum += sensorAvg;
      }

      double finalGlobalAvg = totalSum / vectorSize;
      end_time(avg_op);
      std::cout << "\n--- GLOBAL SPATIAL AVERAGE ---" << std::endl;
      std::cout << "Average value across all " << vectorSize
                << " sensors: " << finalGlobalAvg << std::endl;
      std::cout << "------------------------------" << std::endl;

      std::cout << "[CKKS] Average Division time: " << time_duration_ms(avg_op)
                << " ms" << std::endl;
    } else {
      // For 'add' and 'multi', we just print the raw decrypted values
      for (size_t i = 0; i < 10; i++) {
        std::cout << "Index " << i << ": " << values[i] << std::endl;
      }
    }
  }
  return 0;
}

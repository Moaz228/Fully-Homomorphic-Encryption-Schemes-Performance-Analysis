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

    if (fileExists("cryptocontext.bin") && fileExists("public_key.bin")) {
      std::cout << "[BGV] Loading existing keys..." << std::endl;
      Serial::DeserializeFromFile("cryptocontext.bin", cc, SerType::BINARY);
      Serial::DeserializeFromFile("public_key.bin", pk, SerType::BINARY);
    } else {
      std::cout << "[BGV] Generating new context..." << std::endl;
      CCParams<CryptoContextBGVRNS> params;
      params.SetPlaintextModulus(786433);
      params.SetMultiplicativeDepth(2);

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

      Serial::SerializeToFile("cryptocontext.bin", cc, SerType::BINARY);
      Serial::SerializeToFile("public_key.bin", pk, SerType::BINARY);
      Serial::SerializeToFile("secret_key.bin", sk, SerType::BINARY);

      std::ofstream multKeyFile("mult_key.bin", std::ios::binary);
      cc->SerializeEvalMultKey(multKeyFile, SerType::BINARY);
      std::ofstream rotKeyFile("rot_key.bin", std::ios::binary);
      cc->SerializeEvalAutomorphismKey(rotKeyFile, SerType::BINARY);
    }

    std::vector<int64_t> gateway_input;
    std::ifstream inputFile("sensor_readings.txt");
    double temp_val;
    while (inputFile >> temp_val)
      gateway_input.push_back((int64_t)(temp_val * 100));

    if (gateway_input.empty())
      gateway_input.assign(500, 2500);
    gateway_input.resize(cc->GetRingDimension(), 0);

    Plaintext pt = cc->MakePackedPlaintext(gateway_input);
    start_time(enc);
    auto ct = cc->Encrypt(pk, pt);
    end_time(enc);

    Serial::SerializeToFile("ciphertext_out.bin", ct, SerType::BINARY);
    std::cout << "[BGV] Encryption Time: " << time_duration_ms(enc) << " ms"
              << std::endl;

  } else {
    std::string op = (argc > 2) ? argv[2] : "add";
    CryptoContext<DCRTPoly> cc;
    PrivateKey<DCRTPoly> sk;
    Ciphertext<DCRTPoly> ct;

    Serial::DeserializeFromFile("cryptocontext.bin", cc, SerType::BINARY);
    Serial::DeserializeFromFile("secret_key.bin", sk, SerType::BINARY);
    Serial::DeserializeFromFile("ciphertext_in.bin", ct, SerType::BINARY);

    Plaintext result;
    start_time(dec);
    cc->Decrypt(sk, ct, &result);
    end_time(dec);

    size_t vectorSize = 1000;
    result->SetLength(vectorSize);
    auto values = result->GetPackedValue();

    std::cout << "[BGV] Decryption Time: " << time_duration_ms(dec) << " ms"
              << std::endl;

    if (op == "average") {
      start_time(avg);
      double totalSum = 0.0;
      for (size_t i = 0; i < vectorSize; i++) {
        totalSum += (values[i] / 200.0);
      }
      end_time(avg);
      std::cout << "\n--- BGV GLOBAL SPATIAL AVERAGE ---" << std::endl;
      std::cout << "Result: " << totalSum / vectorSize << std::endl;
      std::cout << "[BGV] Average Division Time: " << time_duration_ms(avg)
                << " ms" << std::endl;

    } else if (op == "oldAverage") {
      auto values = result->GetPackedValue();
      // The cloud already multiplied by 0.5 (inv2),
      // so we just divide by the spatial size.
      double finalAvg = (double)values[0] / vectorSize;
      std::cout << "Global Cloud-Computed Average: " << finalAvg << std::endl;
    }

    else {
      for (size_t i = 0; i < 10; i++)
        std::cout << "Index " << i << ": " << values[i] / 100.0 << std::endl;
    }
  }
  return 0;
}

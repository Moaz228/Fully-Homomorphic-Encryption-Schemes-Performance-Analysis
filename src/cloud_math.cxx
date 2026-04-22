#include "openfhe.h"

// --- SERIALIZATION HEADERS FOR ALL 3 SCHEMES ---
#include "ciphertext-ser.h"
#include "cryptocontext-ser.h"
#include "key/key-ser.h"
#include "scheme/bfvrns/bfvrns-ser.h"   // BFV
#include "scheme/bgvrns/bgvrns-ser.h"   // BGV
#include "scheme/ckksrns/ckksrns-ser.h" // CKKS

#include <iostream>
#include <string>

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
  if (argc < 2) {
    std::cerr << "Usage: ./cloud_math.out <operation>" << std::endl;
    return 1;
  }

  std::string op = argv[1];
  CryptoContext<DCRTPoly> cc;
  Ciphertext<DCRTPoly> ct;

  // 1. Load context and ciphertext
  if (!Serial::DeserializeFromFile("cloud_context.bin", cc, SerType::BINARY))
    return 1;
  if (!Serial::DeserializeFromFile("cloud_ciphertext.bin", ct, SerType::BINARY))
    return 1;

  // 2. Load Evaluation Keys (Mult and Rotation)
  // These are required for EvalSum (Average) and EvalMult
  std::ifstream multKeyFile("mult_key.bin", std::ios::binary);
  if (multKeyFile)
    cc->DeserializeEvalMultKey(multKeyFile, SerType::BINARY);

  std::ifstream rotKeyFile("rot_key.bin", std::ios::binary);
  if (rotKeyFile)
    cc->DeserializeEvalAutomorphismKey(rotKeyFile, SerType::BINARY);

  Ciphertext<DCRTPoly> result;
  // 3. Logic Branching based on Operation
  start_time(enc);
  if (op == "add") {
    // For addition, CKKS handles the double 100.0 n
    if (cc->getSchemeId() == SCHEME::CKKSRNS_SCHEME) {
      result = cc->EvalAdd(ct, 100.0);
    } else {
      Plaintext ptScalar = cc->MakePackedPlaintext({100});
      result = cc->EvalAdd(ct, ptScalar);
    }
  } else if (op == "multiply") {
    if (cc->getSchemeId() == SCHEME::CKKSRNS_SCHEME) {
      // CKKS: Just multiply by the double directly
      result = cc->EvalMult(ct, 2.0);
    } else {
      // BFV/BGV: Use the integer logic
      Plaintext ptFactor = cc->MakePackedPlaintext({2});
      result = cc->EvalMult(ct, ptFactor);
    }
  } else if (op == "average") {

    if (cc->getSchemeId() == SCHEME::CKKSRNS_SCHEME) {
      result = cc->EvalSum(ct, 32);
      result = cc->EvalMult(result, 1.0 / 20.0);
    } else {
      result = cc->EvalSum(ct, 32);
    }
  }
  end_time(enc);
  std::cout << "Operation Time: " << time_duration_ms(enc) << " ms"
            << std::endl;
  // 4. Save result
  Serial::SerializeToFile("cloud_result.bin", result, SerType::BINARY);
  return 0;
}

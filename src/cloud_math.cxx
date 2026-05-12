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

/*
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
*/

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: ./cloud_math.out <operation>" << std::endl;
    return 1;
  }

  std::string op = argv[1];
  CryptoContext<DCRTPoly> cc;

  // We now need TWO ciphertext objects
  Ciphertext<DCRTPoly> ct1;
  Ciphertext<DCRTPoly> ct2;

  // 1. Load context
  if (!Serial::DeserializeFromFile("cloud_context.bin", cc, SerType::BINARY)) {
    std::cerr << "Error loading context" << std::endl;
    return 1;
  }

  // 2. Load BOTH ciphertexts (saved by your Flask script)
  if (!Serial::DeserializeFromFile("cloud_ciphertext_1.bin", ct1,
                                   SerType::BINARY)) {
    std::cerr << "Error loading ciphertext 1" << std::endl;
    return 1;
  }
  if (!Serial::DeserializeFromFile("cloud_ciphertext_2.bin", ct2,
                                   SerType::BINARY)) {
    std::cerr << "Error loading ciphertext 2" << std::endl;
    return 1;
  }

  // 3. Load Evaluation Keys (Mult key is crucial for EvalMult between two
  // ciphertexts)
  std::ifstream multKeyFile("mult_key.bin", std::ios::binary);
  if (multKeyFile)
    cc->DeserializeEvalMultKey(multKeyFile, SerType::BINARY);

  std::ifstream rotKeyFile("rot_key.bin", std::ios::binary);
  if (rotKeyFile)
    cc->DeserializeEvalAutomorphismKey(rotKeyFile, SerType::BINARY);

  Ciphertext<DCRTPoly> result;
  start_time(enc);

  // 4. Vector-to-Vector Logic
  if (op == "add") {
    // This adds ct1[index] + ct2[index] for all slots simultaneously
    result = cc->EvalAdd(ct1, ct2);
  } else if (op == "multiply") {
    // This multiplies ct1[index] * ct2[index]
    // Note: For BFV/BGV/CKKS, multiplying two ciphertexts requires the MultKey
    result = cc->EvalMult(ct1, ct2);
  } else if (op == "average") {
    // Example: Add them and divide then the division will happen in the fog
    result = cc->EvalAdd(ct1, ct2);

  } else if (op == "oldAverage") {
    auto added = cc->EvalAdd(ct1, ct2);
    auto schemaId = cc->getSchemeId();
    double divBy2 = 0.5;

    if (schemaId == SCHEME::CKKSRNS_SCHEME) {
      auto ctPairwiseAvg = cc->EvalMult(added, divBy2);
      result = cc->EvalSum(ctPairwiseAvg, 1000);

    } else if (schemaId == SCHEME::BFVRNS_SCHEME ||
               schemaId == SCHEME::BGVRNS_SCHEME) {

      // 1. Get the Plaintext Modulus (t)
      int64_t t = cc->GetCryptoParameters()->GetPlaintextModulus();

      // 2. Calculate the modular inverse of 2: inv2 = (t + 1) / 2
      // This only works if t is odd (which it almost always is in OpenFHE)
      int64_t inv2 = (t + 1) / 2;

      // 3. Calculate the modular inverse of the vector size (e.g., 500)
      // For simplicity, we'll just multiply by inv2 here to show the temporal
      // average and do the spatial sum.
      Plaintext pInv2 = cc->MakePackedPlaintext({inv2});

      // 4. Multiply Ciphertext by the modular inverse of 2
      auto temporalAvg = cc->EvalMult(added, pInv2);

      // 5. Heavy Spatial Sum (Rotations)
      result = cc->EvalSum(temporalAvg, 1024);
    }
  }

  end_time(enc);
  std::cout << "Operation Time: " << time_duration_ms(enc) << " ms"
            << std::endl;
  std::cout << time_duration_ms(enc) << std::endl;

  // 5. Save result
  Serial::SerializeToFile("cloud_result.bin", result, SerType::BINARY);
  return 0;
}

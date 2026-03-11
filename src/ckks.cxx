#include <fstream>
#include <cstdint>
#include <iostream>
#include <vector>
#include <chrono>
#include <openfhe.h>

#include "ciphertext-ser.h"
#include "cryptocontext-ser.h"
#include "key/key-ser.h"
#include "scheme/ckksrns/ckksrns-ser.h" 

#define start_time(name) \
  std::chrono::high_resolution_clock::time_point name##_start = std::chrono::high_resolution_clock::now()
#define end_time(name) \
  std::chrono::high_resolution_clock::time_point name##_end = std::chrono::high_resolution_clock::now()
#define time_duration_ms(name) (std::chrono::duration_cast<std::chrono::nanoseconds>(name##_end - name##_start).count() / 1000000.0)

using namespace lbcrypto;

int main() {
    std::cout << "========================================================\n";
    std::cout << "--- CKKS Gateway Module: Performance Test --- \n";
    std::cout << "========================================================\n\n";

    // 1. SETUP THE CKKS ENGINE
    CCParams<CryptoContextCKKSRNS> params;
    params.SetMultiplicativeDepth(1);
    params.SetScalingModSize(50); // Bits of precision

    CryptoContext<DCRTPoly> cc = GenCryptoContext(params);
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);

    KeyPair<DCRTPoly> keyPair = cc->KeyGen();

    // GATEWAY RESPONSIBILITY: ENCRYPTION
    // Note: We use double (float) directly!
    // --- DYNAMIC DATA LOADING ---
    std::vector<double> gateway_input;
    std::ifstream inputFile("sensor_readings.txt");
    double temp_val; // Note: using double for CKKS

    while (inputFile >> temp_val) {
        // Since Python writes scaled integers (2543), we divide by 100 
        // to get the original decimal (25.43) back for CKKS processing.
        gateway_input.push_back(temp_val / 100.0);
    }
    inputFile.close();

    if (gateway_input.empty()) {
        gateway_input.assign(100, 25.43); 
    } 
    Plaintext gateway_plaintext = cc->MakeCKKSPackedPlaintext(gateway_input);
    
    start_time(enc);
    auto gateway_ciphertext = cc->Encrypt(keyPair.publicKey, gateway_plaintext);
    end_time(enc);
    std::cout << "[CKKS Gateway] Encryption Time: " << time_duration_ms(enc) << " ms\n";

    // CLOUD MOCK (Serialization)
    Serial::SerializeToFile("ckks_ciphertext.bin", gateway_ciphertext, SerType::BINARY);
    
    Ciphertext<DCRTPoly> cloud_received;
    Serial::DeserializeFromFile("ckks_ciphertext.bin", cloud_received, SerType::BINARY);
    
    start_time(cloud_math);
    // Adding 100.0 directly
    auto cloud_result = cc->EvalAdd(cloud_received, 100.0);
    end_time(cloud_math);
    std::cout << "[CKKS Cloud] Math Time: " << time_duration_ms(cloud_math) << " ms\n";

    // GATEWAY RESPONSIBILITY: DECRYPTION
    Plaintext final_plaintext;
    start_time(dec);
    cc->Decrypt(keyPair.secretKey, cloud_result, &final_plaintext);
    end_time(dec);
    
    std::cout << "[CKKS Gateway] Decryption Time: " << time_duration_ms(dec) << " ms\n";
    
    final_plaintext->SetLength(100);
    // GetRealPackedValue() is used for CKKS
    std::cout << "\n[+] SUCCESS! Final CKKS Decrypted Reading: " << final_plaintext->GetRealPackedValue()[0] << "\n";

    return 0;
}
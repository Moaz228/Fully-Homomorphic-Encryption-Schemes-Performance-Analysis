#include <fstream>
#include <cstdint>
#include <iostream>
#include <vector>
#include <chrono>
#include <openfhe.h>

#include "ciphertext-ser.h"
#include "cryptocontext-ser.h"
#include "key/key-ser.h"
#include "scheme/bgvrns/bgvrns-ser.h" 
#include "EncDataContainer.hxx"

#define start_time(name) \
  std::chrono::high_resolution_clock::time_point name##_start = std::chrono::high_resolution_clock::now()
#define end_time(name) \
  std::chrono::high_resolution_clock::time_point name##_end = std::chrono::high_resolution_clock::now()
#define time_duration_ms(name) (std::chrono::duration_cast<std::chrono::nanoseconds>(name##_end - name##_start).count() / 1000000.0)

using namespace lbcrypto;

int main() {
    std::cout << "========================================================\n";
    std::cout << "--- BGV Gateway Module: Performance Test --- \n";
    std::cout << "========================================================\n\n";

    // 1. SETUP THE BGV ENGINE
    CCParams<CryptoContextBGVRNS> params; 
    params.SetMultiplicativeDepth(2);
    params.SetPlaintextModulus(65537);
    
    CryptoContext<DCRTPoly> cc = GenCryptoContext(params);
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);

    KeyPair<DCRTPoly> keyPair = cc->KeyGen();

    // =========================================================
    // GATEWAY RESPONSIBILITY: ENCRYPTION
    // =========================================================
    #include <fstream> // Make sure this is at the very top with other #includes

    std::vector<int64_t> gateway_input;
    std::ifstream inputFile("sensor_readings.txt");
    int64_t temp_val;

    // Read every number inside sensor_readings.txt
    while (inputFile >> temp_val) {
        gateway_input.push_back(temp_val);
    }
    inputFile.close();

    // If the file was empty or missing, we use a fallback so it doesn't crash
    if (gateway_input.empty()) {
        std::cout << "[!] No data in sensor_readings.txt, using default values.\n";
        gateway_input.assign(100, 2543); 
    }
    Plaintext gateway_plaintext = cc->MakePackedPlaintext(gateway_input);
    
    start_time(enc);
    auto gateway_ciphertext = cc->Encrypt(keyPair.publicKey, gateway_plaintext);
    end_time(enc);
    std::cout << "[BGV Gateway] Encryption Time: " << time_duration_ms(enc) << " ms\n";

    // CLOUD MOCK (Serialization included for fair metric)
    Serial::SerializeToFile("bgv_ciphertext.bin", gateway_ciphertext, SerType::BINARY);
    
    Ciphertext<DCRTPoly> cloud_received;
    Serial::DeserializeFromFile("bgv_ciphertext.bin", cloud_received, SerType::BINARY);
    
    std::vector<int64_t> addition_array(100, 10000);
    Plaintext addition_pt = cc->MakePackedPlaintext(addition_array);
    
    start_time(cloud_math);
    auto cloud_result = cc->EvalAdd(cloud_received, addition_pt);
    end_time(cloud_math);
    std::cout << "[BGV Cloud] Math Time: " << time_duration_ms(cloud_math) << " ms\n";

    // GATEWAY RESPONSIBILITY: DECRYPTION
    Plaintext final_plaintext;
    start_time(dec);
    cc->Decrypt(keyPair.secretKey, cloud_result, &final_plaintext);
    end_time(dec);
    
    std::cout << "[BGV Gateway] Decryption Time: " << time_duration_ms(dec) << " ms\n";
    
    final_plaintext->SetLength(100);
    std::cout << "\n[+] SUCCESS! Final BGV Decrypted Reading: " << final_plaintext->GetPackedValue()[0] / 100.0 << "\n";

    return 0;
}
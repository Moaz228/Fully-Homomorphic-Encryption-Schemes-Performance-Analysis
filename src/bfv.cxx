#include <fstream>
#include <cstdint>
#include <iostream>
#include <vector>
#include <chrono>
#include <openfhe.h>

// OpenFHE Serialization Headers
#include "ciphertext-ser.h"
#include "cryptocontext-ser.h"
#include "key/key-ser.h"
#include "scheme/bfvrns/bfvrns-ser.h"

#include "EncDataContainer.hxx"

#define start_time(name) \
  std::chrono::high_resolution_clock::time_point name##_start = std::chrono::high_resolution_clock::now()

#define end_time(name) \
  std::chrono::high_resolution_clock::time_point name##_end = std::chrono::high_resolution_clock::now()

#define time_duration(name) std::chrono::duration_cast<std::chrono::nanoseconds>(name##_end - name##_start)
#define time_duration_ms(name) (time_duration(name).count() / 1000000.0)

using namespace lbcrypto;

int main() {
    std::cout << "========================================================\n";
    std::cout << "--- BFV Gateway Module: Serialization Edition ---\n";
    std::cout << "========================================================\n\n";

    // ---------------------------------------------------------
    // 1. SETUP THE CRYPTOGRAPHIC ENGINE
    // ---------------------------------------------------------
    CCParams<CryptoContextBFVRNS> params;
    params.SetPlaintextModulus(65537);
    params.SetMultiplicativeDepth(2);
    
    CryptoContext<DCRTPoly> cc = GenCryptoContext(params);
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);

    KeyPair<DCRTPoly> keyPair = cc->KeyGen();
    cc->EvalMultKeyGen(keyPair.secretKey);

    // =========================================================
    // YOUR RESPONSIBILITY: FHE ENCRYPTION & EXPORT
    // =========================================================
    std::cout << "[Gateway] 1. Simulating AES Decryption... (Got 100 readings)\n";
    // --- DYNAMIC DATA LOADING ---
    std::vector<int64_t> gateway_input;
    std::ifstream inputFile("sensor_readings.txt");
    int64_t temp_val;

    while (inputFile >> temp_val) {
        gateway_input.push_back(temp_val);
    }
    inputFile.close();

    if (gateway_input.empty()) {
        gateway_input.assign(100, 2543); // Fallback
    } 
    
    std::cout << "[Gateway] 2. FHE Encrypting...\n";
    Plaintext gateway_plaintext = cc->MakePackedPlaintext(gateway_input);
    
    start_time(enc);
    Ciphertext<DCRTPoly> gateway_ciphertext = cc->Encrypt(keyPair.publicKey, gateway_plaintext);
    end_time(enc);
    std::cout << "  -> Encryption Time: " << time_duration_ms(enc) << " ms\n";

    std::cout << "[Gateway] 3. Serializing Ciphertext to 'ciphertext_out.bin'...\n";
    start_time(ser);
    Serial::SerializeToFile("ciphertext_out.bin", gateway_ciphertext, SerType::BINARY);
    end_time(ser);
    std::cout << "  -> Serialization Time: " << time_duration_ms(ser) << " ms\n\n";

    // =========================================================
    // CLOUD FRIEND'S RESPONSIBILITY (Mocked just to test)
    // =========================================================
    std::cout << "[Cloud] Receiving 'ciphertext_out.bin', Adding 10000, Saving 'ciphertext_in.bin'...\n\n";
    Ciphertext<DCRTPoly> cloud_received_cipher;
    Serial::DeserializeFromFile("ciphertext_out.bin", cloud_received_cipher, SerType::BINARY);
    
    std::vector<int64_t> addition_array(100, 10000);
    Plaintext addition_pt = cc->MakePackedPlaintext(addition_array);
    Ciphertext<DCRTPoly> cloud_result = cc->EvalAdd(cloud_received_cipher, addition_pt);
    
    Serial::SerializeToFile("ciphertext_in.bin", cloud_result, SerType::BINARY);

    // =========================================================
    // YOUR RESPONSIBILITY: IMPORT & FHE DECRYPTION
    // =========================================================
    std::cout << "[Gateway] 4. Reading Cloud response from 'ciphertext_in.bin'...\n";
    Ciphertext<DCRTPoly> gateway_received_cipher;
    
    start_time(deser);
    Serial::DeserializeFromFile("ciphertext_in.bin", gateway_received_cipher, SerType::BINARY);
    end_time(deser);
    std::cout << "  -> Deserialization Time: " << time_duration_ms(deser) << " ms\n";

    std::cout << "[Gateway] 5. FHE Decrypting...\n";
    Plaintext final_plaintext;
    
    start_time(dec);
    cc->Decrypt(keyPair.secretKey, gateway_received_cipher, &final_plaintext);
    end_time(dec);
    std::cout << "  -> Decryption Time: " << time_duration_ms(dec) << " ms\n";

    final_plaintext->SetLength(100);
    double final_value = final_plaintext->GetPackedValue()[0] / 100.0;
    
    std::cout << "\n========================================================\n";
    std::cout << "[+] SUCCESS! Final Decrypted Reading: " << final_value << "\n";
    std::cout << "========================================================\n";

    return EXIT_SUCCESS;
}
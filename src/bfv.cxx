#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <openfhe.h>
#include <EncDataContainer.hxx>
#include <FileWriter.hxx>

#define start_time(name) \
  std::chrono::high_resolution_clock::time_point name##_start \
    = std::chrono::high_resolution_clock::now()

#define end_time(name) \
  std::chrono::high_resolution_clock::time_point name##_end \
    = std::chrono::high_resolution_clock::now()

#define time_duration(name) std::chrono::duration_cast<std::chrono::nanoseconds>(name##_end - name##_start)

static const std::uint64_t inp_len = 100;
static const std::uint64_t num_inp = 10;
static const std::uint64_t iterations = 100;
static const std::pair<std::uint64_t, std::uint64_t> rand_range = {1, 9};

std::vector<std::int64_t> gen_rand_vec(std::uint64_t len, std::pair<std::uint64_t, std::uint64_t> range) {
  std::vector<std::int64_t> res;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(range.first, range.second);
  for (std::uint64_t i{0}; i < len; ++i) res.push_back(dist(gen));
  return res;
}

int main() {
  FileWriter::prepare_file_header({num_inp, inp_len});

  lbcrypto::CCParams<lbcrypto::CryptoContextBFVRNS> params;
  params.SetPlaintextModulus(65537);
  params.SetMultiplicativeDepth(2);
  
  lbcrypto::CryptoContext<lbcrypto::DCRTPoly> crypto_context = GenCryptoContext(params);
  crypto_context->Enable(lbcrypto::PKE);
  crypto_context->Enable(lbcrypto::KEYSWITCH);
  crypto_context->Enable(lbcrypto::LEVELEDSHE);
  crypto_context->Enable(bigintdyn::ADVANCEDSHE);

  for(std::uint64_t i{0}; i < iterations; ++i)  {
    std::vector<EncDataContainer> inputs;
    for(std::uint64_t i{0}; i < num_inp; ++i) {
      EncDataContainer enc_data_container;
      enc_data_container.m_InputVec = gen_rand_vec(inp_len, rand_range);
      inputs.push_back(std::move(enc_data_container));
    }
    FileWriter::output_file <<std::format("{},", i);
    for(std::uint64_t j{0}; j < num_inp; ++j)
      for(std::uint64_t k{0}; k < inp_len; ++k)
        FileWriter::output_file <<std::format("{},", inputs.at(j).m_InputVec.at(k));

    for(std::uint64_t i{0}; i < num_inp; ++i){
      inputs.at(i).m_Plaintext = crypto_context->MakePackedPlaintext(inputs.at(i).m_InputVec);
    }

      //OUTOUT:
    for(std::uint64_t j{0}; j < num_inp; ++j) std::cout <<"Input " <<j <<": " <<inputs.at(j).m_Plaintext <<'\n';

    std::cout <<"Results of homomorphic computations:\n";

    start_time(key_pair_gen);
      lbcrypto::KeyPair key_pair = crypto_context->KeyGen();
    end_time(key_pair_gen);

    //OUTOUT
    std::cout <<"Key Pair Genrration Time(ns): " <<time_duration(key_pair_gen) <<'\n';

    start_time(relin_gen);
      crypto_context->EvalMultKeyGen(key_pair.secretKey);
    end_time(relin_gen);

    //OUTOUT
    std::cout <<"Relinearization Key Genrration Time(ns): " <<time_duration(relin_gen) <<'\n';
    
    start_time(rot_key_gen);
      crypto_context->EvalRotateKeyGen(key_pair.secretKey, {1, 2, -1, -2});
    end_time(rot_key_gen);

    //OUTOUT
    std::cout <<"Rotation Key Genrration Time(ns): " <<time_duration(rot_key_gen) <<'\n';

    for(std::uint64_t j{0}; j < num_inp; ++j) {
      start_time(enc);
        inputs.at(j).m_Ciphertext = crypto_context->Encrypt(key_pair.publicKey, inputs.at(j).m_Plaintext);
      end_time(enc);

      //OUTOUT
      std::cout <<"Input " <<j <<" Encryption Time(ns): " <<time_duration(enc) <<'\n';
      FileWriter::output_file <<std::format("{},", time_duration(enc).count());
    }

    std::vector<lbcrypto::Ciphertext<lbcrypto::DCRTPoly>> inputs_ciphertext(num_inp);
    for(std::uint64_t j{0}; j < num_inp; ++j) inputs_ciphertext.at(j) = (inputs.at(j).m_Ciphertext);

    start_time(homomorphic_add);
      lbcrypto::Ciphertext ciphertext_add = crypto_context->EvalAddMany(inputs_ciphertext);
    end_time(homomorphic_add);

    //OUTOUT
    std::cout <<"Homomorphic Addation Time(ns): " <<time_duration(homomorphic_add) <<'\n';

    // start_time(homomorphic_sub);
    //   lbcrypto::Ciphertext ciphertext_sub = crypto_context->EvalSub(inputs_ciphertext.at(0), inputs_ciphertext.at(1));
    // end_time(homomorphic_sub);

    start_time(homomorphic_mul);
      lbcrypto::Ciphertext ciphertext_mul = crypto_context->EvalMultMany(inputs_ciphertext);
    end_time(homomorphic_mul);
    //OUTOUT
    std::cout <<"Homomorphic Multiplication Time(ns): " <<time_duration(homomorphic_mul) <<'\n';

    // start_time(homomorphic_rot);
    //   lbcrypto::Ciphertext ciphertext_rot = crypto_context->EvalRotate(inputs_ciphertext.at(0), 1);
    // end_time(homomorphic_rot);


    lbcrypto::Plaintext plaintext_add_result;
    start_time(dec_add);
      crypto_context->Decrypt(key_pair.secretKey, ciphertext_add, &plaintext_add_result);
    end_time(dec_add);
    plaintext_add_result->SetLength(inp_len);
    //OUTOUT
    std::cout <<"Decryption of the Added values Time(ns): " <<time_duration(dec_add) <<'\n';

    // lbcrypto::Plaintext plaintext_sub_result;
    // start_time(dec_sub);
    //   crypto_context->Decrypt(key_pair.secretKey, ciphertext_sub, &plaintext_sub_result);
    // end_time(dec_sub);
    // plaintext_sub_result->SetLength(inp_len);
    
    lbcrypto::Plaintext plaintext_mult_result;
    start_time(dec_mul);
      crypto_context->Decrypt(key_pair.secretKey, ciphertext_mul, &plaintext_mult_result);
    end_time(dec_mul);
    plaintext_mult_result->SetLength(inp_len);
    //OUTOUT
    std::cout <<"Decryption of the Multiplied values Time(ns): " <<time_duration(dec_mul) <<'\n';
    
    // lbcrypto::Plaintext plaintext_rot_result;
    // start_time(dec_rot);
    //   crypto_context->Decrypt(key_pair.secretKey, ciphertext_rot, &plaintext_rot_result);
    // end_time(dec_rot);
    // plaintext_rot_result->SetLength(inp_len);
    std::cout <<'\n';

    FileWriter::output_file <<std::format("{},", time_duration(key_pair_gen).count());
    FileWriter::output_file <<std::format("{},", time_duration(relin_gen).count());
    FileWriter::output_file <<std::format("{},", time_duration(rot_key_gen).count());
    FileWriter::output_file <<std::format("{},", time_duration(homomorphic_add).count());
    FileWriter::output_file <<std::format("{},", time_duration(homomorphic_mul).count());
    FileWriter::output_file <<std::format("{},", time_duration(dec_add).count());
    FileWriter::output_file <<std::format("{}\n", time_duration(dec_mul).count());
  }
  return EXIT_SUCCESS;
}

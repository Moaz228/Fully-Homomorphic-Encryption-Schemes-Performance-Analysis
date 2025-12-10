#pragma once

#include <cstdint>
#include <fstream>
#include <string_view>
#include <vector>
#include <string>


namespace FileWriter {
  const static std::vector<std::string_view> time_capture = {
    "key_pair_gen_time",
    "relinearization_key_gen_time",
    "rot_key_gen_time",
    "homomorphic_add_time",
    "homomorphic_mul_time",
    "dec_of_the_added_vals_time",
    "dec_of_the_multiplied_vals_time"
  };

  extern std::fstream output_file;
  std::vector<std::string> get_inputs_header_names(std::vector<std::uint64_t> feilds_lens, std::string prefix="");
  std::vector<std::string> get_file_header_names(std::vector<std::uint64_t> inp_len);
  void prepare_file_header(std::vector<std::uint64_t> fields_lens);
}

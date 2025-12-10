#include <format>
#include <fstream>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <chrono>
#include <FileWriter.hxx>

std::fstream FileWriter::output_file(std::format("output_{}.csv", std::chrono::system_clock::now().time_since_epoch().count()), std::ios::out);

std::vector<std::string> FileWriter::get_inputs_header_names(std::vector<std::uint64_t> fields_lens, std::string prefix) {
  if(!prefix.empty()) prefix += "_";
  std::vector<std::string> result;
  if(fields_lens.empty()) return std::vector<std::string>();
  std::vector<std::string> post_result = get_inputs_header_names(std::vector(fields_lens.begin()+1, fields_lens.end()));

  if(post_result.empty()) for(std::uint64_t i{1}; i <= fields_lens.at(0); ++i) result.push_back(std::format("{}{}", prefix, i));
  else for(std::uint64_t i{1}; i <= fields_lens.at(0); ++i) for(std::uint64_t j{0}; j < post_result.size(); ++j) result.push_back(std::format("{}{}_{}", prefix, i, post_result.at(j)));
  return result;
}

std::vector<std::string> FileWriter::get_file_header_names(std::vector<std::uint64_t> fields_lens) {
  std::vector<std::string> result;
  result.push_back("iteration");

  std::vector<std::string> input_headers_names = get_inputs_header_names(fields_lens, "inp");
  result.insert(result.end(), input_headers_names.begin(), input_headers_names.end());

  for(std::uint64_t i{1}; i <= fields_lens.at(0); ++i) result.push_back(std::format("inp_{}_enc_time", i));
  for(std::uint64_t i{0}; i < time_capture.size(); ++i) result.push_back(std::string(time_capture.at(i)));

  return result;
}

void FileWriter::prepare_file_header(std::vector<std::uint64_t> fields_lens) {
  std::vector<std::string> headers = get_file_header_names(fields_lens);
  for(std::uint64_t i{0}; i < headers.size(); ++i) output_file <<(i!=headers.size()-1?std::format("{},", headers.at(i)):std::format("{}\n", headers.at(i)));
}

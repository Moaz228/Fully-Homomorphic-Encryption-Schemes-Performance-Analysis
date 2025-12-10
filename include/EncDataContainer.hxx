#pragma once

#include <vector>
#include <openfhe.h>

class EncDataContainer {
public:
  std::vector<std::int64_t> m_InputVec;
  lbcrypto::Plaintext m_Plaintext;
  lbcrypto::Ciphertext<lbcrypto::DCRTPoly> m_Ciphertext;

  EncDataContainer() = default;
  EncDataContainer(EncDataContainer &&) = default;
  EncDataContainer(const EncDataContainer &) = default;
  EncDataContainer &operator=(EncDataContainer &&) = default;
  EncDataContainer &operator=(const EncDataContainer &) = default;
  ~EncDataContainer() = default;
};

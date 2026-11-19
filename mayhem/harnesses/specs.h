// Embedded FIX data dictionaries. build.sh generates spec_<NAME>.inc (the comma-separated bytes of
// spec/<NAME>.xml) into its build dir; a harness includes it inside an array initialiser, so no
// harness reads an absolute path at run time.
#pragma once
#include "DataDictionary.h"
#include <memory>
#include <sstream>
#include <string>

inline std::shared_ptr<FIX::DataDictionary> loadSpec(const char *xml, size_t len) {
  std::istringstream ss(std::string(xml, len));
  return std::make_shared<FIX::DataDictionary>(ss);
}

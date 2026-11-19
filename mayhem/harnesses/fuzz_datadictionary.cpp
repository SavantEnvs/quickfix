// FIX data-dictionary XML: DataDictionary(std::istream&) through the bundled pugixml parser and
// readFromDocument (fields, values, messages, components, nested groups). A dictionary that
// loads is then used to parse and validate a fixed message.
#include <cstdint>
#include <exception>
#include <sstream>
#include <string>

#include "DataDictionary.h"
#include "Message.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  std::istringstream ss(std::string(reinterpret_cast<const char *>(data), size));
  try {
    FIX::DataDictionary dd(ss);
    static const std::string msg =
        "8=FIX.4.2\x01" "9=73\x01" "35=D\x01" "34=2\x01" "49=TW\x01" "52=20150513-10:00:00\x01"
        "56=ISLD\x01" "11=ID\x01" "21=1\x01" "40=1\x01" "54=1\x01" "38=100\x01" "55=INTC\x01"
        "10=000\x01";
    try {
      FIX::Message m(msg, dd, false);
      dd.validate(m);
    } catch (const FIX::Exception &) {
    }
  } catch (const FIX::Exception &) {
  } catch (const std::exception &) {
  }
  return 0;
}

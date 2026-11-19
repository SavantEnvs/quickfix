// The admin HttpServer's request parsing: HttpParser::readHttpMessage framing over the received
// bytes, then HttpMessage(const std::string&) (request line, URL, query parameters, unescaping).
#include <cstdint>
#include <string>

#include "HttpMessage.h"
#include "HttpParser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  FIX::HttpParser parser;
  parser.addToStream(reinterpret_cast<const char *>(data), size);
  std::string s;
  try {
    for (int i = 0; i < 64 && parser.readHttpMessage(s); ++i) {
      try {
        FIX::HttpMessage m(s);
        std::string root = m.getRootString();
        std::string params = m.getParameterString();
        std::string out = m.toString();
        for (const auto &kv : m.getParameters()) {
          (void)m.hasParameter(kv.first);
        }
      } catch (const FIX::Exception &) {
      }
    }
  } catch (const FIX::Exception &) {
  }
  return 0;
}

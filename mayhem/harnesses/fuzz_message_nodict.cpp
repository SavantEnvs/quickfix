// FIX message parse without a dictionary: Message(const std::string&, validate) (the
// dictionary-less header/trailer split and BodyLength/CheckSum checks), setStringHeader,
// re-serialisation, reverseRoute, and the field convertors (IntConvertor, DoubleConvertor,
// UtcTimeStampConvertor, ...) applied to every parsed value as a typed getter would.
#include <cstdint>
#include <string>

#include "FieldConvertors.h"
#include "Message.h"

static void convertAll(const FIX::FieldMap &map) {
  for (auto it = map.begin(); it != map.end(); ++it) {
    const std::string &v = it->getString();
    int i;
    double d;
    char c;
    bool b;
    (void)FIX::IntConvertor::convert(v, i);
    (void)FIX::DoubleConvertor::convert(v, d);
    (void)FIX::CharConvertor::convert(v, c);
    (void)FIX::BoolConvertor::convert(v, b);
    try { (void)FIX::UtcTimeStampConvertor::convert(v); } catch (const FIX::Exception &) {}
    try { (void)FIX::UtcTimeOnlyConvertor::convert(v); } catch (const FIX::Exception &) {}
    try { (void)FIX::UtcDateConvertor::convert(v); } catch (const FIX::Exception &) {}
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const std::string s(reinterpret_cast<const char *>(data), size);
  for (bool validate : {true, false}) {
    try {
      FIX::Message m(s, validate);
      convertAll(m.getHeader());
      convertAll(m);
      convertAll(m.getTrailer());
      std::string out = m.toString();
      FIX::Message r;
      r.reverseRoute(m.getHeader());
      (void)r.toString();
      FIX::MsgType type;
      if (m.getHeader().getFieldIfSet(type)) {
        (void)FIX::Message::isAdminMsgType(type);
      }
    } catch (const FIX::Exception &) {
    }
  }
  try {
    FIX::Message h;
    if (h.setStringHeader(s)) {
      (void)h.toString();
    }
    (void)FIX::identifyType(s);
  } catch (const FIX::Exception &) {
  }
  return 0;
}

// FIX message parse + dictionary validation: Message::setString with a DataDictionary
// (header/body/trailer split, repeating groups, BodyLength/CheckSum, FieldConvertors such as
// IntConvertor on every typed field), then DataDictionary::validate and re-serialisation.
// The dictionary follows the message's own BeginString (8=), as the engine's session does.
#include <cstdint>
#include <string>

#include "Message.h"
#include "specs.h"

static const char kSpec_FIX42[] = {
#include "spec_FIX42.inc"
};
static const char kSpec_FIX44[] = {
#include "spec_FIX44.inc"
};
static const char kSpec_FIXT11[] = {
#include "spec_FIXT11.inc"
};
static const char kSpec_FIX50SP2[] = {
#include "spec_FIX50SP2.inc"
};

// Loading the four dictionaries takes seconds under ASan, so it happens once, before the first
// input; afterwards they are only read.
static std::shared_ptr<FIX::DataDictionary> dd42, dd44, ddT11, dd50;

extern "C" int LLVMFuzzerInitialize(int *, char ***) {
  dd42 = loadSpec(kSpec_FIX42, sizeof(kSpec_FIX42));
  dd44 = loadSpec(kSpec_FIX44, sizeof(kSpec_FIX44));
  ddT11 = loadSpec(kSpec_FIXT11, sizeof(kSpec_FIXT11));
  dd50 = loadSpec(kSpec_FIX50SP2, sizeof(kSpec_FIX50SP2));
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const std::string s(reinterpret_cast<const char *>(data), size);
  const FIX::DataDictionary *sess = dd44.get();
  const FIX::DataDictionary *app = dd44.get();
  if (s.rfind("8=FIXT.1.1\x01", 0) == 0) {
    sess = ddT11.get();
    app = dd50.get();
  } else if (s.rfind("8=FIX.4.2\x01", 0) == 0) {
    sess = app = dd42.get();
  }

  FIX::Message m;
  try {
    m.setString(s, true, sess, app);
  } catch (const FIX::Exception &) {
    return 0;
  }
  try {
    FIX::DataDictionary::validate(m, sess, app);
  } catch (const FIX::Exception &) {
  }
  try {
    std::string out = m.toString();
    std::string xml = m.toXML();
    FIX::MsgSeqNum seq;
    m.getHeader().getFieldIfSet(seq);
    FIX::Message reparsed;
    reparsed.setString(out, false, sess, app);
  } catch (const FIX::Exception &) {
  }
  return 0;
}

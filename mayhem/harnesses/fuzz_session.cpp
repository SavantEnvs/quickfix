// FIX session layer as an acceptor sees a peer's byte stream: Parser::readFixMessage framing
// (BodyLength extraction) feeding Session::next, i.e. logon, heartbeat/test request, resend
// request, sequence reset, gap handling, reject generation and message verification.
// Setup mirrors upstream's src/C++/test/SessionTestCase.cpp (FIX.4.2, TW <-> ISLD).
// A fresh Session (with a fresh MemoryStore) is built per input, so no state carries over;
// the dictionary and the fixed clock are read-only.
#include <cstdint>
#include <string>

#include "Application.h"
#include "MessageStore.h"
#include "Parser.h"
#include "Responder.h"
#include "Session.h"
#include "specs.h"

static const char kSpec_FIX42[] = {
#include "spec_FIX42.inc"
};

namespace {
struct NullResponder : public FIX::Responder {
  bool send(const std::string &) override { return true; }
  void disconnect() override {}
};
} // namespace

static std::shared_ptr<FIX::DataDictionary> dd42;

extern "C" int LLVMFuzzerInitialize(int *, char ***) {
  dd42 = loadSpec(kSpec_FIX42, sizeof(kSpec_FIX42));
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static const FIX::UtcTimeStamp now(10, 0, 0, 13, 5, 2015);

  FIX::NullApplication application;
  FIX::MemoryStoreFactory factory;
  NullResponder responder;
  FIX::SessionID sessionID(FIX::BeginString("FIX.4.2"), FIX::SenderCompID("ISLD"), FIX::TargetCompID("TW"));
  // start == end: the session is in range all day.
  FIX::TimeRange sessionTime(FIX::UtcTimeOnly(0, 0, 0), FIX::UtcTimeOnly(0, 0, 0));
  FIX::DataDictionaryProvider provider;
  provider.addTransportDataDictionary(sessionID.getBeginString(), dd42);

  FIX::Session session([]() { return now; }, application, factory, sessionID, provider, sessionTime, 30, 0);
  session.setResponder(&responder);
  session.setCheckLatency(false);

  FIX::Parser parser;
  parser.addToStream(reinterpret_cast<const char *>(data), size);
  std::string msg;
  for (int i = 0; i < 256; ++i) {
    try {
      if (!parser.readFixMessage(msg)) {
        break;
      }
      session.next(msg, now);
    } catch (const FIX::Exception &) {
      break;
    }
  }
  try {
    session.next(now);
  } catch (const FIX::Exception &) {
  }
  return 0;
}

// Session settings (.cfg) parsing: Settings' ini-style reader, SessionSettings validation and
// defaults, env-var expansion, SessionID construction, and Dictionary's typed getters.
#include <cstdint>
#include <exception>
#include <sstream>
#include <string>

#include "SessionSettings.h"
#include "TimeRange.h"

static void readTyped(const FIX::Dictionary &d) {
  static const char *ints[] = {"HeartBtInt", "SocketAcceptPort", "SocketConnectPort", "ReconnectInterval",
                               "LogonTimeout", "LogoutTimeout", "MaxLatency"};
  static const char *bools[] = {"ResetOnLogon", "UseDataDictionary", "ValidateFieldsOutOfOrder",
                                "PersistMessages", "CheckLatency", "MillisecondsInTimeStamp"};
  static const char *days[] = {"StartDay", "EndDay"};
  static const char *strs[] = {"ConnectionType", "StartTime", "EndTime", "DataDictionary", "TimeZone"};
  for (const char *k : ints) {
    try { if (d.has(k)) (void)d.getInt(k); } catch (const FIX::Exception &) {}
  }
  for (const char *k : bools) {
    try { if (d.has(k)) (void)d.getBool(k); } catch (const FIX::Exception &) {}
  }
  for (const char *k : days) {
    try { if (d.has(k)) (void)d.getDay(k); } catch (const FIX::Exception &) {}
  }
  for (const char *k : strs) {
    try { if (d.has(k)) (void)d.getString(k, true); } catch (const FIX::Exception &) {}
  }
  try {
    if (d.has("StartTime") && d.has("EndTime")) {
      FIX::UtcTimeOnly start = FIX::UtcTimeOnlyConvertor::convert(d.getString("StartTime"));
      FIX::UtcTimeOnly end = FIX::UtcTimeOnlyConvertor::convert(d.getString("EndTime"));
      (void)FIX::TimeRange::isInRange(start, end, FIX::DateTime(2015, 5, 13, 10, 0, 0, 0));
    }
  } catch (const FIX::Exception &) {
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  std::istringstream ss(std::string(reinterpret_cast<const char *>(data), size));
  try {
    FIX::SessionSettings settings(ss, true);
    readTyped(settings.get());
    for (const auto &id : settings.getSessions()) {
      readTyped(settings.get(id));
      (void)id.toString();
    }
    std::ostringstream out;
    out << settings;
  } catch (const FIX::Exception &) {
  } catch (const std::exception &) {
  }
  return 0;
}

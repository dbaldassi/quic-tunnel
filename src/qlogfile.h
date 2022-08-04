#ifndef QLOGFILE_H
#define QLOGFILE_H

#include <quic/logging/FileQLogger.h>

template<quic::VantagePoint Endpoint>
class QLog : public quic::FileQLogger
{
  std::string_view _path;
public:

  explicit QLog(std::string_view path) noexcept
    : quic::FileQLogger(Endpoint, "quic-tunnel", std::string(path)),
      _path(path)
  {}

  ~QLog() { outputLogsToFile(std::string(_path), true); }

  static auto create(std::string_view path) { return std::make_shared<QLog>(path); }
};


#endif /* QLOGFILE_H */

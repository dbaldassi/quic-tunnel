#ifndef QUIC_CLIENT_H
#define QUIC_CLIENT_H

#include <glog/logging.h>

#include <folly/fibers/Baton.h>
#include <folly/io/async/ScopedEventBaseThread.h>

#include <quic/api/QuicSocket.h>
#include <quic/client/QuicClientTransport.h>
#include <quic/common/BufUtil.h>
#include <quic/logging/FileQLogger.h>

class QLog : public quic::FileQLogger
{
  std::string_view _path;
public:

  explicit QLog(std::string_view path) noexcept
    : quic::FileQLogger(quic::VantagePoint::Client, "TUNNEL IN", std::string(path)),
      _path(path)
  {}

  ~QLog() { outputLogsToFile(std::string(_path), true); }

  static auto create(std::string_view path) { return std::make_shared<QLog>(path); }
};

class QuicClient : public quic::QuicSocket::ConnectionSetupCallback,
		   public quic::QuicSocket::ConnectionCallback,
		   public quic::QuicSocket::ReadCallback,
		   public quic::QuicSocket::WriteCallback,
		   public quic::QuicSocket::DatagramCallback
{
  static constexpr const char * DEFAULT_QLOG_PATH = ".";
  
  std::string _qlog_path;
  std::string _host;
  uint16_t    _port;
  
  std::shared_ptr<quic::QuicClientTransport> _quic_transport;
  std::map<quic::StreamId, quic::BufQueue>   _pending_output;
  std::map<quic::StreamId, uint64_t>         _recv_offsets;
  folly::fibers::Baton                       _start_done;
  quic::StreamId                             _stream_id;
  std::function<void(const std::string&)>    _on_received_callback;
  
  folly::ScopedEventBaseThread _network_thread;
  
  void send_message(quic::StreamId id, quic::BufQueue& data);

public:
  explicit QuicClient(std::string host,
		      uint16_t port,
		      std::string qlog_path=DEFAULT_QLOG_PATH) noexcept;

  void start();
  void send_message_stream(const char * buffer, size_t len);
  void send_message_datagram(const char * buffer, size_t len);

  void set_on_received_callback(std::function<void(const std::string&)> callback) {
    _on_received_callback = callback;
  }
  
  /* ConnectionSetupCallback */
  void onFirstPeerPacketProcessed() noexcept override;
  void onConnectionSetupError(quic::QuicError error) noexcept override;
  void onTransportReady() noexcept override;
  void onReplaySafe() noexcept override;
  void onFullHandshakeDone() noexcept override;

  /* ConnectionCallback */
  void onFlowControlUpdate(quic::StreamId id) noexcept override;
  void onNewBidirectionalStream(quic::StreamId id) noexcept override;
  void onNewUnidirectionalStream(quic::StreamId id) noexcept override;
  void onStopSending(quic::StreamId, quic::ApplicationErrorCode error) noexcept override;
  void onConnectionEnd() noexcept override;
  void onConnectionError(quic::QuicError error)   noexcept override;
  void onConnectionEnd(quic::QuicError error)     noexcept override;
  void onBidirectionalStreamsAvailable(uint64_t)  noexcept override;
  void onUnidirectionalStreamsAvailable(uint64_t) noexcept override;
  // void onAppRateLimited() noexcept override;
  void onKnob(uint64_t, uint64_t, quic::Buf) override;

  /* ReadCallback */
  void readAvailable(quic::StreamId id) noexcept override;
  void readError(quic::StreamId id, quic::QuicError error) noexcept override;
  
  /* Writecallback */
  void onStreamWriteReady(quic::StreamId id, uint64_t) noexcept override;
  void onConnectionWriteReady(uint64_t) noexcept override;
  void onStreamWriteError(quic::StreamId id, quic::QuicError error) noexcept override;
  void onConnectionWriteError(quic::QuicError error) noexcept override;

  /* Datagram */
  void onDatagramsAvailable() noexcept override;
};


#endif /* QUIC_CLIENT_H */

#ifndef MVFST_CLIENT_H
#define MVFST_CLIENT_H

#include <glog/logging.h>

#include <folly/fibers/Baton.h>
#include <folly/io/async/ScopedEventBaseThread.h>

#include <quic/api/QuicSocket.h>
#include <quic/client/QuicClientTransport.h>
#include <quic/common/BufUtil.h>

#include <mutex>

#include "qlogfile.h"

#include "quic_client.h"

class MvfstClient : public QuicClient,
                    public quic::QuicSocket::ConnectionSetupCallback,
                    public quic::QuicSocket::ConnectionCallback,
		    public quic::QuicSocket::ReadCallback,
		    public quic::QuicSocket::WriteCallback,
		    public quic::QuicSocket::DatagramCallback
{
  std::string _qlog_path;
  std::string _qlog_file_name;
  
  std::shared_ptr<quic::QuicClientTransport> _quic_transport;
  std::map<quic::StreamId, quic::BufQueue>   _pending_output;
  std::map<quic::StreamId, uint64_t>         _recv_offsets;
  std::map<quic::StreamId, std::string>      _recv_msg;
  
  folly::SocketAddress         _addr;
  folly::ScopedEventBaseThread _network_thread;
  folly::fibers::Baton         _start_done;  

  quic::StreamId              _stream_id;
  quic::CongestionControlType _cc;

  mutable std::mutex mutex;

private:
  void send_message(quic::StreamId id, quic::BufQueue& data);

public:
  MvfstClient(std::string_view host,
	     uint16_t port,
	     std::string qlog_path=DEFAULT_QLOG_PATH) noexcept;

  ~MvfstClient() = default;

  void set_qlog_filename(std::string name) noexcept override { _qlog_file_name = std::move(name); };
  std::string_view get_qlog_path()     const noexcept { return _qlog_path; }
  std::string_view get_qlog_filename() const noexcept { return _qlog_file_name; }

  /* QuicClient overrides */
  bool set_cc(std::string_view cc) noexcept override;
  void start() override;
  void stop() override;
  void send_message_stream(const char * buffer, size_t len) override;
  void send_message_datagram(const char * buffer, size_t len) override;
  
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

#endif /* MVFST_CLIENT_H */

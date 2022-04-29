#ifndef CALLBACK_HANDLER_H
#define CALLBACK_HANDLER_H

#include <deque>

#include <quic/api/QuicSocket.h>

#include <folly/io/async/EventBase.h>
#include <quic/common/BufUtil.h>

#include "udp_socket.h"

class CallbackHandler : public quic::QuicSocket::ConnectionSetupCallback,
			public quic::QuicSocket::ConnectionCallback,
			public quic::QuicSocket::ReadCallback,
			public quic::QuicSocket::WriteCallback,
			public quic::QuicSocket::DatagramCallback,
			public UdpSocketCallback
{
  folly::EventBase*                 _evb;
  std::shared_ptr<quic::QuicSocket> _transport;
  UdpSocket *                       _udp_socket;
  std::deque<quic::StreamId>        _queue_ids;
  
public:
  CallbackHandler() = default;

  void set_quic_socket(std::shared_ptr<quic::QuicSocket> sock) noexcept {
    _transport = sock;
  }
  void set_evb(folly::EventBase* evb)   noexcept { _evb = evb; }
  void set_udp_socket(UdpSocket * sock) noexcept { _udp_socket = sock; }
  
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
  void onAppRateLimited() noexcept override;
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
  
  /* UdpSocketcallback */
  void onUdpMessage(const char * buffer, size_t len) noexcept override;
};

#endif /* CALLBACK_HANDLER_H */

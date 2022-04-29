
#include "callback_handler.h"

// ConnectionSetupCallback ////////////////////////////////////////////////////
void CallbackHandler::onFirstPeerPacketProcessed() noexcept
{
  LOG(INFO) << "onFirstPeerPacketProcessed";
}

void CallbackHandler::onConnectionSetupError(quic::QuicError error) noexcept
{
  onConnectionError(std::move(error));
}

void CallbackHandler::onTransportReady() noexcept
{
  LOG(INFO) << "onTransportReady";
}

void CallbackHandler::onReplaySafe() noexcept
{
  LOG(INFO) << "onReplaysafe";
}

void CallbackHandler::onFullHandshakeDone() noexcept
{
  LOG(INFO) << "onFullhandshakedone";
}

// ConnectionCallback /////////////////////////////////////////////////////////
void CallbackHandler::onFlowControlUpdate(quic::StreamId id) noexcept
{
  
}

void CallbackHandler::onNewBidirectionalStream(quic::StreamId id) noexcept
{
  // LOG(INFO) << "Out Quic tunnel: new bidirectional stream=" << id;
  _transport->setReadCallback(id, this);
}

void CallbackHandler::onNewUnidirectionalStream(quic::StreamId id) noexcept
{
  // LOG(INFO) << "Out Quic tunnel: new unidirectional stream=" << id;
  _transport->setReadCallback(id, this);
}

void CallbackHandler::onStopSending(quic::StreamId id, quic::ApplicationErrorCode) noexcept
{
  VLOG(10) << "Out Quic tunnel got StopSending stream id=" << id;
}

void CallbackHandler::onConnectionEnd() noexcept
{
  LOG(INFO) << "Connection end";
}

void CallbackHandler::onConnectionError(quic::QuicError error) noexcept
{
  LOG(ERROR) << "EchoClient error: " << toString(error.code)
	     << "; errStr=" << error.message;
}

void CallbackHandler::onConnectionEnd(quic::QuicError error) noexcept
{
  LOG(ERROR) << "OnConnectionEndError: " << toString(error.code)
               << "; errStr=" << error.message;
}

void CallbackHandler::onBidirectionalStreamsAvailable(uint64_t)  noexcept
{
  LOG(INFO) << "onBidirectionalStreamsAvailable";
}

void CallbackHandler::onUnidirectionalStreamsAvailable(uint64_t) noexcept
{
  LOG(INFO) << "onUnidirectionalStreamsAvailable";
}

void CallbackHandler::onAppRateLimited() noexcept
{
  // LOG(INFO) << "onAppRateLimited";
  ConnectionCallback::onAppRateLimited();
}

void CallbackHandler::onKnob(uint64_t, uint64_t, quic::Buf)
{
  LOG(INFO) << "onKnob";
}

// ReadCallback ///////////////////////////////////////////////////////////////
void CallbackHandler::readAvailable(quic::StreamId id) noexcept
{
  constexpr auto MAX_LEN = 0;
  auto data = _transport->read(id, MAX_LEN);
  if(data.hasError()) {
    LOG(ERROR) << "Out Quic tunnel failed read from stream=" << id
	       << ", error=" << (uint32_t)data.error();
  }

  _queue_ids.push_front(id);
  
  auto copy = data->first->clone();
  auto message = copy->moveToFbString().toStdString();

  // LOG(INFO) << "Received message : " << message;
  
  _udp_socket->send(message.c_str(), message.size());
}

void CallbackHandler::readError(quic::StreamId id, quic::QuicError error) noexcept
{
  LOG(ERROR) << "Out Quic tunnel failed read from stream=" << id
	     << ", error=" << toString(error);
  // A read error only terminates the ingress portion of the stream state.
  // Your application should probably terminate the egress portion via
  // resetStream
}
  
// Writecallback //////////////////////////////////////////////////////////////
void CallbackHandler::onStreamWriteReady(quic::StreamId id, uint64_t max_to_send) noexcept
{
  LOG(INFO) << "Out Quic Tunnel socket is write ready with maxToSend="
	    << max_to_send;
}

void CallbackHandler::onConnectionWriteReady(uint64_t) noexcept
{
  
}

void CallbackHandler::onStreamWriteError(quic::StreamId id, quic::QuicError error) noexcept
{
  LOG(ERROR) << "Out quic tunnel write error with stream=" << id
	     << " error=" << toString(error);
}

void CallbackHandler::onConnectionWriteError(quic::QuicError error) noexcept
{

}

// Datagram ///////////////////////////////////////////////////////////////////

void CallbackHandler::onDatagramsAvailable() noexcept
{
  LOG(INFO) << "on Datagrams available";
}

// UdpSocketCallback //////////////////////////////////////////////////////////

void CallbackHandler::onUdpMessage(const char * buffer, size_t len) noexcept
{
  _evb->runInEventBaseThread([this, &buffer, &len]() {
    std::string msg{buffer, len};
    // LOG(INFO) << "OnUDPmessage : " << msg;
    auto id = _queue_ids.back();
    _queue_ids.pop_back();

    auto qbuf = folly::IOBuf::copyBuffer(std::move(msg));

    auto res = _transport->writeChain(id, std::move(qbuf), false);
    if(res.hasError()) {
      LOG(ERROR) << "In quic tunnel write chaine error=" << uint32_t(res.error());
    }
  });
}

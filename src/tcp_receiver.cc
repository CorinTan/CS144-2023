#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if ( !isn && !message.SYN )
    // 未建立TCP连接时直接丢弃非SYN包
    return;
  if ( !isn && message.SYN )
    // 建立TCP连接
    isn = message.seqno;
  const uint64_t check_point = inbound_stream.bytes_pushed() + 1;              // stream index to abs_seqnno
  const uint64_t abs_seqno = message.seqno.unwrap( isn.value(), check_point ); // abs_seqno
  // convert to stream index
  const uint64_t first_index = message.SYN ? 0 : abs_seqno - 1;
  reassembler.insert( first_index, std::move( message.payload ), message.FIN, inbound_stream );
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  TCPReceiverMessage send_msg;  
  if ( !isn ) {
  // 未建立TCP连接
    send_msg.ackno = nullopt;
    send_msg.window_size = u64ToU16(inbound_stream.available_capacity() + 2); // + SYN + FIN
  } else {
  // 已建立TCP连接
    const uint64_t abs_seqno = inbound_stream.bytes_pushed() + 1 + inbound_stream.is_closed(); // stream index to abs_seq index
    send_msg.ackno = Wrap32::wrap(abs_seqno, isn.value());
    send_msg.window_size = u64ToU16(inbound_stream.available_capacity() +  inbound_stream.is_closed() ); // + FIN?
  }
  return send_msg;
}

inline uint16_t TCPReceiver::u64ToU16(uint64_t num_64) const
{
  return num_64 > UINT16_MAX ? UINT16_MAX : num_64;  
}
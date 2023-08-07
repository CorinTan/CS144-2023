#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage &message, Reassembler& reassembler, Writer& inbound_stream )
{
 // 1. Set the Initial Sequence Number if necessary.
 // 2. Push any data to the Reassembler.




}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  // Your code here.
  (void)inbound_stream;
  return {};
}

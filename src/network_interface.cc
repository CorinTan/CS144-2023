#include "network_interface.hh"

#include "arp_message.hh"
#include "frame_ip.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address ), frame_to_send_(), ip_mac()
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  const uint32_t& next_ip = next_hop.ipv4_numeric();
  if ( ip_mac.find( next_ip ) == ip_mac.end() ) {
    // 广播ARP request
    ARPMessage broadcast_arp;
    broadcast_arp.sender_ethernet_address = ethernet_address_;
    broadcast_arp.target_ethernet_address = ETHERNET_BROADCAST;
    broadcast_arp.sender_ip_address = ip_address_.ipv4_numeric();
    broadcast_arp.target_ip_address = next_ip;
    broadcast_arp.opcode = ARPMessage::OPCODE_REQUEST;
    EthernetFrame frame_arp;
    frame_arp.header.src = ethernet_address_;
    frame_arp.header.dst = ETHERNET_BROADCAST;
    frame_arp.header.type = EthernetHeader::TYPE_ARP;
    frame_arp.payload = serialize( broadcast_arp );
    arp_to_send_.push( frame_arp );
    return;
  }

  // encasulate to frame_ip
  EthernetFrame frame_ip;
  frame_ip.header.src = ethernet_address_;
  frame_ip.header.dst = ip_mac[next_ip];
  frame_ip.header.type = EthernetHeader::TYPE_IPv4;
  frame_ip.payload = serialize( dgram );

  ip_to_send_.push( frame_ip );
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{

  const EthernetAddress dst_mac = frame.header.dst;
  if ( dst_mac != ETHERNET_BROADCAST && dst_mac != ethernet_address_ )
    return {}; // 丢弃, 忽略不是发送给自己的帧, 接收广播帧或者自己的帧

  // 接收
  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp_msg;
    parse( arp_msg, frame.payload );

    // 记录和更新发送的ip地址和mac地址
    if ( ip_mac.find( arp_msg.sender_ip_address ) == ip_mac.end() )
      ip_mac.insert( { arp_msg.sender_ip_address, arp_msg.sender_ethernet_address } );
    else
      ip_mac[arp_msg.sender_ip_address] = arp_msg.sender_ethernet_address;

    // 回应arp_request信息
    if ( arp_msg.opcode == ARPMessage::OPCODE_REQUEST ) {
      ARPMessage arp_reply;
      arp_reply.opcode = ARPMessage::OPCODE_REPLY;
      arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
      arp_reply.sender_ethernet_address = ethernet_address_;
      arp_reply.target_ethernet_address = arp_msg.sender_ethernet_address;
      arp_reply.target_ip_address = arp_msg.sender_ip_address;
      // 封装成以太网帧
      EthernetFrame frame_arp_reply;
      frame_arp_reply.header.dst = arp_reply.target_ethernet_address;
      frame_arp_reply.header.src = arp_reply.sender_ethernet_address;
      frame_arp_reply.header.type = EthernetHeader::TYPE_ARP;
      frame_arp_reply.payload = serialize( arp_reply );

      arp_to_send_.push( frame_arp_reply );
    }
    return {}; // 不发送IP数据报
  }
  // 接收ip数据报
  else if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram ip_content;
    if ( parse( ip_content, frame.payload ) )
      return ip_content;
  }
  return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  (void)ms_since_last_tick;
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  optional<EthernetFrame> maybe_send;
  if ( !arp_to_send_.empty() ) {
    maybe_send = arp_to_send_.front();
    arp_to_send_.pop();
  } else if ( !ip_to_send_.empty() ) {
    maybe_send = ip_to_send_.front();
    ip_to_send_.pop();
  }
  return maybe_send;
}

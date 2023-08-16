#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
  , ip_to_send_()
  , frame_to_fill_()
  , arp_to_send_()
  , arp_time()
  , ip_mac()
  , ip_time()
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
  cout << "call send_datagram" << endl;
  const uint32_t& next_ip = next_hop.ipv4_numeric();
  if ( ip_mac.find( next_ip ) == ip_mac.end() ) {
    if ( arp_time.find( next_ip ) == arp_time.end() )
      broadcastARP( next_ip );
    EthernetFrame need_be_filled;
    need_be_filled.header.src = ethernet_address_;
    need_be_filled.header.type = EthernetHeader::TYPE_IPv4;
    need_be_filled.payload = serialize( dgram );
    frame_to_fill_.push( need_be_filled );
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
  cout << "call recv_frame" << endl;
  const EthernetAddress dst_mac = frame.header.dst;
  if ( dst_mac != ETHERNET_BROADCAST && dst_mac != ethernet_address_ )
    return {}; // 丢弃, 忽略不是发送给自己的帧, 接收广播帧或者自己的帧

  // 接收IP数据报
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram ip_datagram;
    if ( parse( ip_datagram, frame.payload ) )
      return ip_datagram;
  }
  // 接收ARP
  else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp_msg;
    parse( arp_msg, frame.payload );
    // 更新ARP表
    updateARPTable( arp_msg.sender_ip_address, arp_msg.sender_ethernet_address );

    // 回应arp_request信息
    if ( arp_msg.opcode == ARPMessage::OPCODE_REQUEST ) {
      if ( arp_msg.target_ip_address != ip_address_.ipv4_numeric() ) // 不是查询自己的mac地址
        return {};

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
    } else if ( arp_msg.opcode == ARPMessage::OPCODE_REPLY ) {
      // 填充发出arp请求的mac帧
      auto frame_valid = frame_to_fill_.front();
      frame_valid.header.dst = arp_msg.sender_ethernet_address;
      ip_to_send_.push( frame_valid );
      frame_to_fill_.pop();
    }
  }
  return {}; // 不发送IP数据报
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  cout << "tick" << endl;
  updateMappingTime( ms_since_last_tick );
  updateArpTime( ms_since_last_tick );
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  cout << "call maybe_send" << endl;
  optional<EthernetFrame> maybe_send;
  if ( !arp_to_send_.empty() ) {
    maybe_send = arp_to_send_.front();
    arp_to_send_.pop(); // lab不用考虑收不到的情况
    ARPMessage arp_msg;
    parse( arp_msg, maybe_send.value().payload );
    arp_time.insert( { arp_msg.target_ip_address, 0 } );
  } else if ( !ip_to_send_.empty() ) {
    maybe_send = ip_to_send_.front();
    ip_to_send_.pop();
  }
  if ( maybe_send )
    cout << "发送:" << maybe_send.value().header.to_string() << endl;
  else
    cout << "发送空帧" << endl;
  return maybe_send;
}

void NetworkInterface::updateMappingTime( const size_t ms_since_last_tick )
{
  cout << "call updateMappingTime" << endl;
  // update mapping table
  auto it = ip_time.begin();
  while ( it != ip_time.end() ) {
    it->second += ms_since_last_tick;
    if ( it->second >= 30000 ) {
      ip_mac.erase( it->first );
      it = ip_time.erase( it );
    } else
      ++it;
  }
}

void NetworkInterface::updateArpTime( const size_t ms_since_last_tick )
{
  cout << "call updateArpTime" << endl;
  auto it = arp_time.begin();
  while ( it != arp_time.end() ) {
    it->second += ms_since_last_tick;
    if ( it->second >= 5000 )
      it = arp_time.erase( it );
    else
      ++it;
  }
}

void NetworkInterface::broadcastARP( uint32_t dst_ip )
{
  cout << "call broadcastARP" << endl;
  // 广播ARP request
  ARPMessage broadcast_arp;
  broadcast_arp.sender_ethernet_address = ethernet_address_;
  // broadcast_arp.target_ethernet_address ; ARP设置为全0
  broadcast_arp.sender_ip_address = ip_address_.ipv4_numeric();
  broadcast_arp.target_ip_address = dst_ip;
  broadcast_arp.opcode = ARPMessage::OPCODE_REQUEST;
  // cout << "发送的广播ARP:" << broadcast_arp.to_string() << endl;
  EthernetFrame frame_arp;
  frame_arp.header.src = ethernet_address_;
  frame_arp.header.dst = ETHERNET_BROADCAST;
  frame_arp.header.type = EthernetHeader::TYPE_ARP;
  frame_arp.payload = serialize( broadcast_arp );
  // cout << "发送的广播帧头:" << frame_arp.header.to_string() << endl;
  arp_to_send_.push( frame_arp );
}

void NetworkInterface::updateARPTable( const uint32_t& ip, const EthernetAddress& mac )
{
  // update ip_mac address
  if ( ip_mac.find( ip ) == ip_mac.end() ) {
    ip_mac.insert( { ip, mac } );
    ip_time.insert( { ip, 0 } );
  } else {
    ip_mac[ip] = mac;
    ip_time[ip] = 0;
  }
}
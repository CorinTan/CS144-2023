#include "router.hh"

#include <iostream>
#include <limits>
#include <vector>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
  RouteItem item_to_add = { route_prefix, prefix_length, next_hop, interface_num };
  route_table.push_back( item_to_add );
}

void Router::route()
{
  for ( auto& inf : interfaces_ ) {
    optional<InternetDatagram> recved_ipdatagram = inf.maybe_receive();
    if ( recved_ipdatagram.has_value() ) {
      uint32_t ip = recved_ipdatagram.value().header.dst;
      optional<RouteItem> route_item = longest_prefix_match( ip );
      auto& ttl = recved_ipdatagram.value().header.ttl;
      if ( !route_item.has_value() || ttl < 2 )
        return; // 匹配失败直接丢弃

      // 修改TTL，更新checksum, 发送
      ttl -= 1;
      recved_ipdatagram.value().header.compute_checksum();
      auto& target_interface = interface( route_item.value().interface_num );
      if ( route_item.value().next_hop.has_value() )
        target_interface.send_datagram( recved_ipdatagram.value(), route_item.value().next_hop.value() );
      else {
        // next_hop为空，应该直接发送目标ip
        Address target_ip = Address::from_ipv4_numeric( recved_ipdatagram.value().header.dst );
        target_interface.send_datagram( recved_ipdatagram.value(), target_ip );
      }
    }
  }
}

optional<RouteItem> Router::longest_prefix_match( const uint32_t ip )
{
  uint8_t last_length = 0;
  optional<RouteItem> longest_item;
  for ( const auto& item : route_table ) {
    uint32_t mask = -1 << ( 32 - item.prefix_length );
    bool check = !( ( ip & mask ) ^ item.route_prefix );
    if ( check && item.prefix_length > last_length ) {
      last_length = item.prefix_length;
      longest_item = item;
    }
  }
  return longest_item;
}
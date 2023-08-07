#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Convert absolute seqno -> seqno
  return zero_point + static_cast<uint32_t>( n );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Convert seqno -> absolute seqno
  uint8_t move_bits;
  for ( move_bits = 0; checkpoint > INT32_MAX; ++move_bits )
    checkpoint >>= 1;
  return static_cast<uint64_t>( zero_point.raw_value_ ) << move_bits;
}

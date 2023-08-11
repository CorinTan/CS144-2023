#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

inline void Timer::reset()
{
  round_time_ = 0;
  uint64_t RTO_ms_ = 0;
  is_running_ = false;
  is_expired_ = false;
}

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms ), retrans_timer_()
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return {};
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return {};
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // Your code here.
  return {};
}

void TCPSender::push( Reader& outbound_stream )
{
  // Your code here.
  (void)outbound_stream;
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // Your code here.
  return {};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  (void)msg;
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  if ( retrans_timer_.is_running() && !retrans_timer_.is_expired() ) {
    retrans_timer_.increase_round_time( ms_since_last_tick );
    if ( retrans_timer_.get_round_time() >= cur_RTO_ms_ )
      retrans_timer_.set_expired();
  }

  if ( !retrans_timer_.is_expired() )
    return;
  
  // 1. 重传最早的TCP段
  
  // 2. 窗口尺寸非0
  //    1. 记录连续重传次数
  //    2. RTO加倍
  
  
  
  // 3. 重置重传定时器并启动它
  retrans_timer_.reset();
  retrans_timer_.start(cur_RTO_ms_);
}

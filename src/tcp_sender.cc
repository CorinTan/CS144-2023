#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <optional>
#include <random>
using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */

TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) )
  , initial_RTO_ms_( initial_RTO_ms )
  , cur_RTO_ms_( initial_RTO_ms )

  , retransmit_( false )
  , syn_send_( false )
  , fin_send_( false )
  , feak_window_( false )

  , next_abs_seqno_( 0 )
  , last_ackno_()
  , window_size_( 1 )
  , send_window_size_( window_size_ )
  , consecutive_retrans_cnt_( 0 )
  , retrans_timer_()
  , segments_to_send_()
  , outstanding_segments_()
  , outstanding_seq_cnt_( 0 )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return outstanding_seq_cnt_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retrans_cnt_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  optional<TCPSenderMessage> seg_maybe_send;
  // 发送缓存中的TCP段
  if ( !segments_to_send_.empty() ) {
    if ( !retransmit_ )
      consecutive_retrans_cnt_ = 0; // 正常发送，连续重传次数置为0
    if ( segments_to_send_.front().sequence_length() <= window_size_ ) {
      seg_maybe_send = segments_to_send_.front();
      segments_to_send_.pop_front();
    }
  }

  if ( seg_maybe_send ) {
    if ( !retrans_timer_.is_running() )
      retrans_timer_.start( cur_RTO_ms_ );
  }
  return seg_maybe_send;
}

void TCPSender::push( Reader& outbound_stream )
{
  TCPSenderMessage seg_to_send;
  seg_to_send.SYN = !syn_send_; // 发送TCP建立请求
  uint64_t need_bytes;
  if ( seg_to_send.SYN )
    need_bytes = 0;
  else
    need_bytes = TCPConfig::MAX_PAYLOAD_SIZE < send_window_size_ ? TCPConfig::MAX_PAYLOAD_SIZE : send_window_size_;

  while ( true ) {
    string payload;
    // 从 outbound_stream 读取相应字节流 : 填满窗口或者无法读到数据（已经发送完或者暂时没有数据可读）
    while ( outbound_stream.bytes_buffered() && payload.size() != need_bytes ) {
      string_view next = outbound_stream.peek();
      uint64_t bytes_to_pop = next.size();
      if ( payload.size() + next.size() > need_bytes )
        bytes_to_pop = need_bytes - payload.size();
      payload += next.substr( 0, bytes_to_pop );
      outbound_stream.pop( bytes_to_pop );
    }
    seg_to_send.payload = payload;
    // 封装TCP段，插入发送队列
    if ( !fin_send_ && seg_to_send.sequence_length() < send_window_size_ )
      seg_to_send.FIN = outbound_stream.is_finished(); // 读取后关闭
    if ( seg_to_send.sequence_length() && seg_to_send.sequence_length() <= send_window_size_ ) {
      if ( seg_to_send.SYN )
        syn_send_ = true;
      if ( seg_to_send.FIN )
        fin_send_ = true;
      seg_to_send.seqno = isn_ + next_abs_seqno_;
      send_window_size_ -= seg_to_send.sequence_length();
      next_abs_seqno_ += seg_to_send.sequence_length();
      segments_to_send_.push_back( seg_to_send );     // 性能ok：只复制了智能指针
      outstanding_segments_.push_back( seg_to_send ); // 追踪发出的tcp段
      outstanding_seq_cnt_ += seg_to_send.sequence_length();
    }
    if ( send_window_size_ == 0 || !outbound_stream.bytes_buffered() )
      break;
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // 不占据序列号，也不追踪和重发
  Wrap32 seqno = isn_ + next_abs_seqno_;
  return { seqno, false, {}, false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  TCPSenderMessage front;
  uint64_t front_abs_seqno;
  uint64_t lower_bound;

  if ( msg.ackno ) {
    const uint64_t abs_ackno = msg.ackno.value().unwrap( isn_, next_abs_seqno_ );
    const uint64_t abs_last_ackno = last_ackno_ ? last_ackno_.value().unwrap( isn_, next_abs_seqno_ ) : 0;
    if ( abs_ackno > next_abs_seqno_ || abs_ackno < abs_last_ackno )
      return; // 无效ack
    if ( !outstanding_segments_.empty() ) {
      front = outstanding_segments_.front();
      front_abs_seqno = ( front.seqno + front.sequence_length() ).unwrap( isn_, next_abs_seqno_ );
    } else
      front_abs_seqno = UINT64_MAX;
    lower_bound = msg.ackno.value().unwrap( isn_, next_abs_seqno_ );
    if ( last_ackno_ != msg.ackno && lower_bound < front_abs_seqno )
      return; // 无效ack
  }

  // 有效ack, 更新窗口信息
  feak_window_ = msg.window_size == 0;
  window_size_ = feak_window_ ? 1 : msg.window_size;
  send_window_size_ = window_size_;
  if ( msg.ackno ) {
    last_ackno_ = msg.ackno.value();                                   // 更新ackno
    lower_bound = last_ackno_.value().unwrap( isn_, next_abs_seqno_ ); // 更新lower_bound
    cur_RTO_ms_ = initial_RTO_ms_;                                     // 重置RTO
    consecutive_retrans_cnt_ = 0;                                      // 重置连续重传计数器
    bool popped = false;                                               // 是否有效接收
    while ( !outstanding_segments_.empty() ) {                         // 移除buffer中已经被确认的segments
      front = outstanding_segments_.front();
      front_abs_seqno = ( front.seqno + front.sequence_length() ).unwrap( isn_, next_abs_seqno_ );
      if ( front_abs_seqno <= lower_bound ) {
        outstanding_seq_cnt_ -= outstanding_segments_.front().sequence_length();
        outstanding_segments_.pop_front();
        popped = true;
      } else
        break;
    }
    if ( outstanding_segments_.empty() ) {
      retrans_timer_.stop(); // 所有segment都被接收， 停止timer
      retransmit_ = false;
    } else if ( popped )
      retrans_timer_.restart( cur_RTO_ms_ ); // 重启定时器
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  if ( retrans_timer_.is_running() ) {
    retrans_timer_.increase_round_time( ms_since_last_tick );
    // 重传最早的TCP段
    if ( retrans_timer_.is_expired() ) {
      segments_to_send_.push_front( outstanding_segments_.front() ); // 收到ack才pop
      retransmit_ = true;
      if ( window_size_ ) {
        ++consecutive_retrans_cnt_; // 记录连续重传次数, 没有连续重传的时候要置为0
        if ( !feak_window_ )
          cur_RTO_ms_ *= 2;
      }
      retrans_timer_.restart( cur_RTO_ms_ );
    }
  }
}

/* Timer function definations */

inline void Timer::reset()
{
  round_time_ = 0;
  RTO_ms_ = 0;
  is_running_ = false;
  is_expired_ = false;
}

inline void Timer::start( const uint64_t cur_RTO_ms )
{
  is_running_ = true;
  RTO_ms_ = cur_RTO_ms;
}

inline void Timer::stop()
{
  is_running_ = false;
}

inline void Timer::restart( const uint64_t cur_RTO_ms )
{
  reset();
  start( cur_RTO_ms );
}

inline bool Timer::is_running() const
{
  return is_running_;
}

inline bool Timer::is_expired() const
{
  return is_expired_;
}

inline uint64_t Timer::get_round_time() const
{
  return round_time_;
}

inline void Timer::increase_round_time( const size_t ms )
{
  round_time_ += ms;
  if ( round_time_ >= RTO_ms_ && !is_expired_ )
    is_expired_ = true;
}
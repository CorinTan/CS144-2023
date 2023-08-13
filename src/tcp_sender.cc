#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <iostream>
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

  , next_abs_seqno_( 0 )
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
    seg_maybe_send = segments_to_send_.front();
    segments_to_send_.pop_front();
  }

  if ( seg_maybe_send ) {
    if ( !retrans_timer_.is_running() )
      retrans_timer_.start( cur_RTO_ms_ );
    /* cout << "要发送 seq=" << seg_maybe_send.value().seqno.unwrap( isn_, next_abs_seqno_ )
         << " SYN=" << seg_maybe_send.value().SYN << " FIN=" << seg_maybe_send.value().FIN
         << " payload_size=" << seg_maybe_send.value().payload.length()
         << " segment_size=" << seg_maybe_send.value().sequence_length() << endl; */
  }
  return seg_maybe_send;
}

void TCPSender::push( Reader& outbound_stream )
{
  TCPSenderMessage seg_to_send;
  seg_to_send.SYN = !syn_send_;                                  // 发送TCP建立请求
  seg_to_send.FIN = !fin_send_ && outbound_stream.is_finished(); // 读取前已关闭
  uint64_t need_bytes;
  if ( seg_to_send.SYN || seg_to_send.FIN )
    need_bytes = 0;
  else
    need_bytes = TCPConfig::MAX_PAYLOAD_SIZE < send_window_size_ ? TCPConfig::MAX_PAYLOAD_SIZE : send_window_size_;

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

  // 封装TCP段，插入发送队列
  seg_to_send.FIN = !fin_send_ && outbound_stream.is_finished(); // 读取后关闭
  seg_to_send.payload = payload;
  if ( seg_to_send.sequence_length() && seg_to_send.sequence_length() <= send_window_size_ ) {
    if ( seg_to_send.SYN )
      syn_send_ = true;
    if (seg_to_send.FIN)
      fin_send_ = true;
    seg_to_send.seqno = isn_ + next_abs_seqno_;
    send_window_size_ -= seg_to_send.sequence_length();
    next_abs_seqno_ += seg_to_send.sequence_length();
    segments_to_send_.push_back( seg_to_send );     // 性能ok：只复制了智能指针
    outstanding_segments_.push_back( seg_to_send ); // 追踪发出的tcp段
    outstanding_seq_cnt_ += seg_to_send.sequence_length();
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // 不占据序列号，也不追踪和重发
  // cout << "要发送 empty : " << next_abs_seqno_ << endl;
  Wrap32 seqno = isn_ + next_abs_seqno_;
  return { seqno, false, {}, false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.ackno ) {
    const uint64_t abs_ackno = msg.ackno.value().unwrap( isn_, next_abs_seqno_ );
    if ( abs_ackno > next_abs_seqno_ )
      return; // 非法ack
  }
  window_size_ = msg.window_size;
  send_window_size_ = window_size_ ? window_size_ : 1;
  if ( msg.ackno ) {
    cur_RTO_ms_ = initial_RTO_ms_; // 重置RTO
    consecutive_retrans_cnt_ = 0;  // 重置连续重传计数器
    while ( !outstanding_segments_.empty() ) { // 移除buffer中已经被确认的segments
      const auto& front = outstanding_segments_.front();
      const uint64_t front_abs_seqno = front.seqno.unwrap( isn_, next_abs_seqno_ );
      const uint64_t lower_bound = msg.ackno.value().unwrap( isn_, next_abs_seqno_ );
      if ( front_abs_seqno < lower_bound ) {
        outstanding_seq_cnt_ -= outstanding_segments_.front().sequence_length();
        outstanding_segments_.pop_front();
      } else
        break;
    }
    if ( outstanding_segments_.empty() ) {
      retrans_timer_.stop(); // 所有segment都被接收， 停止timer
      retransmit_ = false;
    } else
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
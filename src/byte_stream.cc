#include "byte_stream.hh"
// #include <iostream>
#include <stdexcept>

using namespace std;

/* ByteStream: */

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), available_capacity_( capacity_ ) {}

/* Writer: */

void Writer::push( string data )
{
  // 空数据
  if ( data.empty() ) {
    return;
  }
  // 已关闭
  else if ( closed_ ) {
    // cerr << "The byteStream is closed!" << endl;
    return;
  }
  // 已经出错
  else if ( has_error_ ) {
    // cerr << "The byteStream had an error!" << endl;
    return;
  }
  // 没有容量
  else if ( available_capacity() == 0 ) {
    // cerr << "No enough capacity to write!" << endl;
    return;
  }
  // 写入
  else {
    const uint64_t len = data.size();
    const uint64_t write_len = len < available_capacity() ? len : available_capacity();
    if ( write_len < len ) {
      data = data.substr( 0, write_len );
      /* string err_msg = "No enough capacity, Write data : ";
      ( err_msg += to_string( write_len ) += '/' ) += to_string( len );
      cerr << err_msg << endl; */
    }
    pipe_string_.push_back( std::move( data ) );
    pipe_view_.emplace_back( pipe_string_.back() );
    total_pushed_ += write_len;
    available_capacity_ -= write_len;
  }
}

void Writer::close()
{
  // check all data popped.
  /* if ( total_popped_ != total_pushed_ ) {
    const string msg = "There is some data unpopped!";
    std::cerr << msg << std::endl;
  }
  std::cout << "The pipe is closed!" << std::endl; */
  closed_ = true;
}

void Writer::set_error()
{
  has_error_ = true;
}

bool Writer::is_closed() const
{
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  return available_capacity_;
}

uint64_t Writer::bytes_pushed() const
{
  return total_pushed_;
}

string_view Reader::peek() const
{
  if ( pipe_view_.empty() ) {
    return {};
  }
  return pipe_view_.front();
}

/* Reader : */

bool Reader::is_finished() const
{
  return closed_ && !bytes_buffered();
}

bool Reader::has_error() const
{
  return has_error_;
}

void Reader::pop( uint64_t len )
{

  const uint64_t left = bytes_buffered();
  uint64_t pop_len = len < left ? len : left;

  // 不足量
  // if ( len > left )
  //   cerr << "Will pop " << pop_len << "/" << len << "bytes!" << endl;
  // 读数据
  while ( pop_len > 0 ) {
    uint64_t cur_pop_len = pipe_view_.front().length();
    if ( pop_len >= cur_pop_len ) {
      // pop当前view
      pipe_view_.pop_front();
      pipe_string_.pop_front();
      pop_len -= cur_pop_len;
    } else {
      cur_pop_len = pop_len;
      pipe_view_.front().remove_prefix( pop_len );
      pop_len = 0;
    }
    total_popped_ += cur_pop_len;
    available_capacity_ += cur_pop_len;
  }
}

uint64_t Reader::bytes_buffered() const
{
  return total_pushed_ - total_popped_;
}

uint64_t Reader::bytes_popped() const
{
  return total_popped_;
}

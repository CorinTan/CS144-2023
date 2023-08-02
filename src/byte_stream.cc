#include "byte_stream.hh"
#include <iostream>
#include <stdexcept>

using namespace std;

/* ByteStream: */

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), available_capacity_( capacity_ ) {}

/* Writer: */

void Writer::push( std::string &data )
{
  // 空数据
  if ( data.empty() )
    return;

  // 已关闭
  if ( closed_ ) {
    const string error = "The byteStream is closed!";
    set_error( error );
  }
  // 已经出错
  else if ( has_error_ ) {
    const string error = "The byteStream had an error!";
    set_error( error );
  }
  // 超出可写入容量，不能写入
  else if ( data.size() > available_capacity() ) {
    const string error = "No enough capacity to write!";
    set_error( error );
  }
  // 写入
  else {
    for ( char byte : data )
      pipe_.push( byte );
    total_pushed_ += data.size();
    available_capacity_ -= data.size();
  }
}

void Writer::close()
{
  // check all data popped.
  if ( total_popped_ != total_pushed_ ) {
    const string msg = "There is some data unpopped!";
    std::cerr << msg << std::endl;
  }
  std::cout << "The pipe is closed!" << std::endl;
  closed_ = true;
}

void Writer::set_error( std::string_view err )
{
  std::cerr << err << std::endl;
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
  // string_view next_byte = pipe_.pop();

  return
}

/* Reader : */

bool Reader::is_finished() const
{
  // Your code here.
  return {};
}

bool Reader::has_error() const
{
  return has_error_;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  (void)len;
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return {};
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return {};
}

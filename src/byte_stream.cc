#include <stdexcept>
#include <iostream>
#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), 
    closed_( false ), error_( false ), total_pushed_(0), total_popped_(0), available_capacity_(capacity_) { }

void Writer::push(std::string_view data )
{
  // 已关闭
  if (closed_)
  {
    const string error = "The byteStream is closed!";
    set_error(error);
  }
  // 已经出错
  else if (error_)
  {
    const string error = "The byteStream had an error!";
    set_error(error);
  }
  // 超出可写入容量，不能写入
  else if ( data.size() > available_capacity() )
  {
    const string error = "No enough capacity to write!";
    set_error(error);
  }
  // 写入
  else
  {
    for (char byte : data)
      pipe_.push(byte);
    total_pushed_ += data.size();
    available_capacity_ -= data.size();
  }
}

void Writer::close()
{
  // check all data popped.
  closed_ = true;
}

void Writer::set_error(std::string_view err)
{
  std::cerr << err << std::endl;
  error_ = true;
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

bool Reader::is_finished() const
{
  // Your code here.
  return {};
}

bool Reader::has_error() const
{
  return error_;
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

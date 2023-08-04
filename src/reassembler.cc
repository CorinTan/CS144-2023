#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  next_need_index = output.bytes_pushed();
  Strobj segment( first_index, std::move( data ), is_last_substring );

  // 什么时候会被丢弃
  // 什么时候会被立即发送? 当前string 包含要开始的字节流
  // 什么时候会被存储
  
  
  
  
  if ( toSend( segment, output ) ) {
    // 立即 push to writer
    output.push( segment.data );
    if ( segment.is_last_substring )
      output.close();
  } else {
  
  }

else
  // 重叠或者错误TCP段，直接丢弃
  return;
}

uint64_t Reassembler::bytes_pending() const
{
  return total_bytes_pending;
}

bool Reassembler::toSend( Strobj& bytes, Writer& output )
{
  auto available_capacity = output.available_capacity();
  if (available_capacity == 0)
    return false;
  uint64_t start = bytes.first_index;
  uint64_t end = start + bytes.data.length() - 1;
  if ( next_need_index < start || next_need_index > end )
    return false;
  auto send_lenth = std::min( end - next_need_index + 1, available_capacity);
  bytes.data = std::move( bytes.data.substr( next_need_index - start, send_lenth ) );
    return true;
}

Strobj::Strobj( uint64_t index, string&& content, bool is_last )
  : first_index( index ), data( content ), is_last_substring( is_last )
{}

Strobj::Strobj( Strobj&& strobj )
{
  first_index = strobj.first_index;
  data = std::move( strobj.data );
  is_last_substring = strobj.is_last_substring;
}

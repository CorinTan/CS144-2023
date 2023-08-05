#include "reassembler.hh"

using namespace std;

/* Class Reassembler functions */
void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  next_need_index = output.bytes_pushed();
  Strobj segment( first_index, std::move( data ), is_last_substring );
  auto available_capacity = output.available_capacity();

  // discard:
  if ( outOfBound( segment, available_capacity ) )
    return;
  // push to writer immediately
  else if ( sendNow( segment, available_capacity ) ) {
    next_need_index += segment.data.length();
    output.push( segment.data );
    if ( segment.is_last_substring )
      output.close();
    // buffer第一个obj是否能被push
    if ( !segment.is_last_substring && buffer_domains.front().first <= next_need_index
         && next_need_index <= buffer_domains.front().second )
      popFirstDomain( output );
  }
  // store internally
  else
    storeInternally( std::move( segment ), output );
}

uint64_t Reassembler::bytes_pending() const
{
  return total_bytes_pending;
}

bool Reassembler::outOfBound( const Strobj& segment, uint64_t available_capacity )
{
  // The pipe is full or index out of bound.
  return ( available_capacity == 0 ) || ( segment.first_index > next_need_index + available_capacity -1 )
         || ( segment.first_index + segment.data.length() - 1 < next_need_index );
}

bool Reassembler::sendNow( Strobj& segment, uint64_t avaiable_capacity )
{
  if (segment.first_index > next_need_index)
    return false;
  else if (segment.first_index == next_need_index && segment.data.length() <= avaiable_capacity)
    return true;  // 不更改segment, 直接发送
  else {  // 更改segment, 在发送
    auto str_lenth = std::min( avaiable_capacity, segment.data.length() );
    segment.data = segment.data.substr( next_need_index-segment.first_index, str_lenth );
    return true;
  }
}

void Reassembler::storeInternally( Strobj&& segment, Writer& output )
{
  auto start = segment.first_index;
  auto end = segment.first_index + segment.data.length() - 1;
  // Insert to the right postion.

  // 空直接插入
  if ( buffer_domains.empty() ) {
    buffer_domains.insert( buffer_domains.end(), { start, end } );
    buffer_strobj.insert( { start, std::move( segment ) } );
    return;
  }
  // 非空插入: 查找插入位置
  auto it = buffer_domains.begin();
  while ( it != buffer_domains.end() ) {
    if ( start <= it->first ) {
      auto old_start = it->first;
      auto old_end = it->second;
      it->first = start, it->second = end;
      buffer_domains.insert( it, { old_start, old_end } );
      buffer_strobj.insert( { start, std::move( segment ) } );
      break;
    }
  }
  if ( it == buffer_domains.end() ) {
    buffer_domains.insert( it, { start, end } );
    buffer_strobj.insert( { start, std::move( segment ) } );
  }

  // 合并重叠区间
  mergeBufferDomain();

  // buffer第一个obj是否能被push
  if ( buffer_domains.front().first <= next_need_index && next_need_index <= buffer_domains.front().second ) {
    popFirstDomain( output );
  }
}

void Reassembler::mergeBufferDomain()
{
  auto it1 = buffer_domains.begin();
  auto it2 = it1;
  ++it2;
  while ( it2 != buffer_domains.end() ) {
    if ( it2->first <= it1->first ) // 重叠、合并
    {
      if ( it2->second <= it1->second ) // it2被包含，直接删除
      {
        buffer_strobj.erase( it2->first );
        buffer_domains.erase( it2 );
      } else // if (it2->second > it1->second), 合并区间
      {
        // 合成新的segment
        string new_data = std::move( buffer_strobj[it1->first].data );
        string join_data = buffer_strobj[it2->first].data;
        join_data = std::move( join_data.substr( it1->second - it2->first + 1, it2->second - it1->second ) );
        new_data += join_data;
        bool last = buffer_strobj[it1->first].is_last_substring | buffer_strobj[it2->first].is_last_substring;
        Strobj new_segment( it1->first, std::move( new_data ), last );

        // 修改原有数据结构，并插入新的segment
        buffer_strobj.erase( it1->first );
        buffer_strobj.erase( it2->first );
        buffer_strobj.insert( { new_segment.first_index, std::move( new_segment ) } );
        it1->second = it2->second;
        buffer_domains.erase( it2 );
      }
    }
    // 移动迭代器
    it2 = ++it1;
    ++it2;
  }
}

void Reassembler::popFirstDomain( Writer& output )
{
  auto it = buffer_domains.begin();
  output.push( buffer_strobj[it->first].data );
  if ( buffer_strobj[it->first].is_last_substring )
    output.close();
  buffer_domains.erase( it );
}

/* Class Strobj functions */
Strobj::Strobj( uint64_t index, string&& content, bool is_last ) noexcept
  : first_index( index ), data( std::move( content ) ), is_last_substring( is_last )
{}

Strobj::Strobj( Strobj&& strobj ) noexcept
{
  first_index = strobj.first_index;
  data = std::move( strobj.data );
  is_last_substring = strobj.is_last_substring;
}

#include "reassembler.hh"

using namespace std;

/* Class Reassembler functions */
void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  updateBounds( output );
  // discard:
  if ( outOfBound( first_index, data ) )
    return;

  // push to writer immediately
  if ( sendNow( first_index, data ) )
    pushToWriter( data, output, is_last_substring );

  // store internally
  else
    insertBuffer( first_index, data, is_last_substring );
  popValidDomains( output );
}

inline void Reassembler::pushToWriter( const string& data, Writer& output, const bool last )
{
  output.push( data );
  updateBounds( output );
  if ( last )
    output.close();
}

inline void Reassembler::updateBounds( Writer& output )
{
  // [ )
  lower_bound = output.bytes_pushed();
  upper_bound = lower_bound + output.available_capacity();
}

uint64_t Reassembler::bytes_pending() const
{
  return total_bytes_pending;
}

bool Reassembler::outOfBound( const uint64_t first_index, const string& data )
{
  // The pipe is full or index out of bound.
  return ( !data.empty() && lower_bound == upper_bound ) || ( first_index >= upper_bound )
         || ( first_index + data.length() < lower_bound );
}

bool Reassembler::sendNow( const uint64_t first_index, string& data )
{
  if ( first_index > lower_bound )
    return false;
  if ( first_index == lower_bound )
    return true; // 直接发送
  else {
    // 更改segment, 再发送。只用截取前部分，后部分会被 Writer 截取
    data = data.substr( lower_bound - first_index );
    return true;
  }
}

void Reassembler::insertBuffer( uint64_t first_index, string& data, bool is_last_substring )
{
  // [start, end)
  uint64_t start = first_index;
  uint64_t end = start + data.length();

  // 空直接插入
  if ( buffer_domains.empty() ) {
    // cout << "buffer is empty." << endl;
    // 检查要放入buffer的字节是否超出
    if (first_index + data.length() > upper_bound) {
      data = data.substr(0, first_index + data.length() - upper_bound);
      end = upper_bound;
    }
    total_bytes_pending += data.length();
    buffer_domains.insert( buffer_domains.end(), { start, end } );
    buffer_data.insert( { start, { data, is_last_substring } } );
    return;
  }

  // 非空插入: 查找插入位置
  auto it = buffer_domains.begin();
  while ( it != buffer_domains.end() ) {
    if ( start < it->first ) {
      // 插入区间和map, 修改pending
      it = buffer_domains.insert( it, { start, end } );
      total_bytes_pending += data.length();
      buffer_data.insert( { start, { std::move( data ), is_last_substring } } );
      break;
    } else if ( start == it->first && end > it->second ) {
      // 更新, 扩大旧区间, 修改map和pending
      uint64_t diff_bytes = end - it->second;
      total_bytes_pending += diff_bytes;
      it->second = end;
      buffer_data[start] = { std::move( data ), buffer_data[start].second | is_last_substring };
      break;
    } else if ( start == it->first && end <= it->second )
      // 被包含在已有区间，不做处理
      return;
    else
      ++it;
  }
  if ( it == buffer_domains.end() ) {
    it = buffer_domains.insert( it, { start, end } );
    total_bytes_pending += data.length();
    buffer_data.insert( { start, { std::move(data), is_last_substring } } );
  }
  mergerBuffer();
}

void Reassembler::mergerBuffer()
{
  if (buffer_domains.empty())
    return ;
  auto p = buffer_domains.begin();
  auto post = p;
  ++post;
  while (post != buffer_domains.end()) {
    if ( p->second >= post->second) {
      // 包含，删除post
      buffer_data.erase(post->first);
      total_bytes_pending -= post->second - post->first;
      post = buffer_domains.erase(post); // 删除并移动
    }
    else if (p->second >= post->first && p->second < post->second) {
      // 交错
      string new_data = std::move(buffer_data[p->first].first);
      string joint_data = std::move(buffer_data[post->first].first);
      total_bytes_pending -= p->second - post->first;
      joint_data = joint_data.substr(p->second-post->first);
      new_data += joint_data;
      // 更新map
      buffer_data[p->first].first = std::move(new_data);
      buffer_data[p->first].second = buffer_data[p->first].second | buffer_data[post->first].second;
      buffer_data.erase(post->first);
      // 更新domain
      p->second = post->second;
      post = buffer_domains.erase(post); // 删除并移动
    }
    else
      ++p, ++post;
  }
}

void Reassembler::popValidDomains( Writer& output )
{
  // 删除无效区间
  while ( !buffer_domains.empty() && buffer_domains.front().second <= lower_bound ) {
    buffer_data.erase( buffer_domains.front().first );
    total_bytes_pending -= buffer_domains.front().second - buffer_domains.front().first;
    buffer_domains.pop_front();
  }

  // 发送有效区间
  while ( !buffer_domains.empty() && buffer_domains.front().first <= lower_bound ) {
    uint64_t start = buffer_domains.front().first;
    uint64_t end = buffer_domains.front().second; // [ )
    if ( buffer_data[start].first.length() < lower_bound - start )
      lower_bound = lower_bound;
    bool last = buffer_data[start].second;
    string to_send = std::move( buffer_data[start].first );
    // 处理要传输的字符串，存储的都是有效区间
    if ( lower_bound > start ) // 前部截断
      to_send = to_send.substr( lower_bound - start );

    pushToWriter( to_send, output, last );
    buffer_data.erase( start );
    total_bytes_pending -= end - start;
    buffer_domains.pop_front();
  }
}
#include "byte_stream.hh"
#include <sstream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity), _left(capacity), 
    _data(vector<char>(capacity)) { 
}

size_t ByteStream::write(const string &data) {
    // if(data.size() > _left) {
    //     _error = 0;
    // }
    size_t n = data.size() > _left ? _left : data.size();

    _left -= n;
    _written += n;

    for(size_t i=0; i<n; ++i)
    {
        _data[_front] = data[i];
        _front = (_front + 1) % _capacity;
    }
    return n;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    ostringstream oss;
    size_t n = len > buffer_size() ? buffer_size() : len;

    size_t r = _rear;
    while(n--)
    {
        oss << _data[r];
        r = (r + 1) % _capacity;
    }

    return oss.str();
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    size_t n = len > buffer_size() ? buffer_size() : len;

    _left += n;
    _read += n;
    _rear = (_rear + n) % _capacity;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    ostringstream oss;
    size_t n = len > buffer_size() ? buffer_size() : len;

    _left += n;
    _read += n;
    while(n--)
    {
        oss << _data[_rear];
        _rear = (_rear + 1) % _capacity;
    }

    return oss.str();
}

void ByteStream::end_input() { _end_input=true;}

bool ByteStream::input_ended() const { return _end_input;}

size_t ByteStream::buffer_size() const { return _capacity-_left; }

bool ByteStream::buffer_empty() const { return _left==_capacity; }

bool ByteStream::eof() const { return buffer_size()==0 && _end_input; }

size_t ByteStream::bytes_written() const { return _written; }

size_t ByteStream::bytes_read() const { return _read; }

size_t ByteStream::remaining_capacity() const { return _left; }

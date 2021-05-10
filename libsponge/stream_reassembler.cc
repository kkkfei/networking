#include "stream_reassembler.hh"
#include <sstream>
#include <iostream>

#include <algorithm>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), 
    _data(vector<int>(capacity, 256)){}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t cap =  _output.remaining_capacity();
    size_t maxIdx = _frontIdx + cap;
    size_t dataLastIdx = index + data.size();

    if(eof) {
        _eof = eof;
        _lastIdx = dataLastIdx;

        if(_frontIdx == _lastIdx) _output.end_input();
    }

    if(dataLastIdx <= _frontIdx || index >= maxIdx || cap == 0) return;

    size_t stringStartIdx = 0;
    size_t dataStartIdx = index;
    if(index < _frontIdx) {
        stringStartIdx = _frontIdx - index;
        dataStartIdx = _frontIdx;
    }

    //std::cout << data << "," << index << ", " << _frontIdx << endl;



    size_t strIdx = (_front + (dataStartIdx - _frontIdx)) % _capacity;
    size_t n = maxIdx >= dataLastIdx ? dataLastIdx - dataStartIdx : maxIdx - dataStartIdx;
    for(size_t i=0; i<n; ++i)
    {
        if(_data[strIdx] == 256) ++_unassembled;
        _data[strIdx] = data[stringStartIdx + i];
        strIdx = (strIdx + 1) % _capacity;
    }

    if(dataStartIdx == _frontIdx) {
        size_t cnt = 0;
        size_t old_front = _front;

        while(cnt < cap && _data[_front] != 256)
        {
            //oss << char(_data[_front]);
            //_data[_front] = 256;
            _front = (_front + 1) % _capacity;
            ++cnt;
        }

        string s{};
        s.reserve(cnt);
        if(old_front < _front)
        {
            s.append(_data.cbegin()+old_front, _data.cbegin()+_front);
            fill_n(_data.begin() + old_front, cnt, 256);
        } else {
            size_t n1 = _capacity - old_front;
            s.append(_data.cbegin()+old_front, _data.cend());
            fill_n(_data.begin() + old_front, n1, 256);

            s.append(_data.cbegin(), _data.cbegin() + _front);
            fill_n(_data.begin(), cnt - n1, 256);
        }

        _output.write(s);
        _frontIdx += cnt;
        _unassembled -= cnt;
        if(_eof && _frontIdx == _lastIdx) _output.end_input();
    }

    // ostringstream oss;
    // if(dataStartIdx == _frontIdx) {
    //     size_t cnt = 0;
    //     while(cnt < cap && _data[_front] != 256)
    //     {
    //         oss << char(_data[_front]);
    //         _data[_front] = 256;
    //         _front = (_front + 1) % _capacity;
    //         ++cnt;
    //     }
    //     _output.write(oss.str());
    //     _frontIdx += cnt;
    //     _unassembled -= cnt;
    //     if(_eof && _frontIdx == _lastIdx) _output.end_input();
    // }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled; }

bool StreamReassembler::empty() const { return _unassembled == 0; }

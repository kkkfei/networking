#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {

    bool syn = seg.header().syn;
    bool fin = seg.header().fin;

    if(syn) {
        _isn = seg.header().seqno;
        _syn = true;
    }


    if(_syn) {
        if(fin) _fin = true;

        uint64_t abs_seqno = unwrap(seg.header().seqno, _isn, _checkPoint);
        _reassembler.push_substring(seg.payload().copy(), abs_seqno - (syn? 0 : 1), fin);
        _opt = wrap(_reassembler._frontIdx + (_reassembler._output.input_ended() ? 2 : 1), _isn);
        _checkPoint = abs_seqno;
    }

}

optional<WrappingInt32> TCPReceiver::ackno() const { return _opt; }

size_t TCPReceiver::window_size() const { 
    return _reassembler.stream_out().remaining_capacity(); 
}

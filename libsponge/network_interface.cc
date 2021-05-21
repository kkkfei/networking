#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";

    _map[ip_address.ipv4_numeric()] = ethernet_address;
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    auto ite = _map.find(next_hop_ip); 
    // if ip is known, send dgram
    if(ite != _map.end())
    {
        EthernetFrame frame;
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.header().src = _ethernet_address;
        frame.header().dst = ite->second;
        frame.payload() = dgram.serialize();

        _frames_out.emplace(std::move(frame));
    } 
    else {
        // save message

        // request arp
        ARPMessage arpMsg;
        arpMsg.opcode = ARPMessage::OPCODE_REQUEST;
        arpMsg.sender_ethernet_address = _ethernet_address;
        arpMsg.sender_ip_address = _ip_address.ipv4_numeric();   
        arpMsg.target_ip_address = next_hop_ip;

        EthernetFrame frame;
        frame.header().type = EthernetHeader::TYPE_ARP;
        frame.header().src = _ethernet_address;
        frame.header().dst = ETHERNET_BROADCAST;
        frame.payload() = arpMsg.serialize();
        _frames_out.emplace(std::move(frame));


        _waitData.push_back({dgram, next_hop});
    }


    //DUMMY_CODE(dgram, next_hop, next_hop_ip);
}

void NetworkInterface::cacheAddress(uint32_t ip_address, const EthernetAddress &ethernet_address)
{
    auto ite = _map.find(ip_address); 
    // if ip is known, send dgram
    if(ite == _map.end())
    {
        _map[ip_address] = ethernet_address;

        for(auto& data : _waitData)
        {
            if(data.next_hop.ipv4_numeric() == ip_address)
            {
                send_datagram(data.dgram, data.next_hop);
            }
        }
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    
    if(frame.header().dst != ETHERNET_BROADCAST &&  frame.header().dst != _ethernet_address)
        return {};

    switch (frame.header().type)
    {
        case EthernetHeader::TYPE_ARP:
        {
            ARPMessage msg;
            if(msg.parse(frame.payload()) != ParseResult::NoError) break;

            if(msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == _ip_address.ipv4_numeric())
            {
                ARPMessage arpMsg;
                arpMsg.opcode = ARPMessage::OPCODE_REPLY;
                arpMsg.sender_ethernet_address = _ethernet_address;
                arpMsg.sender_ip_address = _ip_address.ipv4_numeric();   
                arpMsg.target_ip_address = msg.sender_ip_address;
                arpMsg.target_ethernet_address = msg.target_ethernet_address;

                EthernetFrame f;
                f.header().type = EthernetHeader::TYPE_ARP;
                f.header().src = _ethernet_address;
                f.header().dst = frame.header().src;
                f.payload() = arpMsg.serialize();
                _frames_out.emplace(std::move(f));

            } else if(msg.opcode == ARPMessage::OPCODE_REPLY)
            {
                cacheAddress(msg.target_ip_address , msg.target_ethernet_address);
                cacheAddress(msg.sender_ip_address , msg.sender_ethernet_address);
            }
        }
        break;

        case EthernetHeader::TYPE_IPv4:
        {
            InternetDatagram id;
            if(id.parse(frame.payload()) == ParseResult::NoError)
            {
                return {id};
            }
        }
        break;
        
        default:
            break;
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

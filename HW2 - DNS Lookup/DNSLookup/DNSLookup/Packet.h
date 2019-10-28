#pragma once
#include "pch.h"
#include <iostream>
#include <stdio.h>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <string>
#include <sstream>
#include <vector>

typedef unsigned short	ushort;
typedef unsigned int	uint;
typedef signed int		sint;
typedef unsigned char	uchar;
typedef signed char		schar;

#define DNS_A 1			/* name -> IP */
#define DNS_NS 2		/* name server */
#define DNS_CNAME 5		/* canonical name */
#define DNS_PTR 12		/* IP -> name */
#define DNS_HINFO 13	/* host info/SOA */
#define DNS_MX 15		/* mail exchange */
#define DNS_AXFR 252	/* request for zone transfer */
#define DNS_ANY 255		/* all records */


#pragma pack(push, 1)

//https://stackoverflow.com/questions/8584577/access-bits-in-a-char-in-c
struct flag_bits {
ushort result : 4;
ushort reserved : 3;
ushort rcr_av : 1;
ushort rcr_d : 1;
ushort tc : 1;
ushort aa : 1;
ushort opcode : 4;
ushort query : 1;
};

struct dns_flags {
	union {
		ushort flags;
		struct flag_bits bits;
	};
};

struct UDP_Packet {
	//here is the DNS portion
	ushort txid;
	struct dns_flags flags;
	ushort nQuestions;
	ushort nAnswers;
	ushort nAuthority;
	ushort nAdditional;
	char variable[492];
};

struct compressed_jump {
	union {
		struct {
			ushort lower14 : 14;
			ushort upper2 : 2;
		};
		ushort num;
	};
	
	
};


class Packet {
public:
	struct UDP_Packet packet;
	struct UDP_Packet sent_packet;

	std::string lookup_string;
	uint var_ind;
	uint resp_ind;

	std::vector<std::string> questions;
	std::vector<std::string> answers;
	std::vector<std::string> authorities;
	std::vector<std::string> additional;
	
	


	Packet(ushort txid, std::vector<std::string> host_parts, uint queryType);

	//get individual flags
	ushort getQuery()		{return packet.flags.bits.query;	}
	ushort getOpcode()		{return packet.flags.bits.opcode;	}
	ushort getAA()			{return packet.flags.bits.aa;		}
	ushort getTC()			{return packet.flags.bits.tc;		}
	ushort getRD()			{return packet.flags.bits.rcr_d;	}
	ushort getRA()			{return packet.flags.bits.rcr_av;	}
	ushort getReserved()	{return packet.flags.bits.reserved;	}
	ushort getResult()		{return packet.flags.bits.result;	}
	ushort getFlags()		{return packet.flags.flags;			}

	ushort getTXID()		{return packet.txid;				}	


	ushort getNQuestions()	{return ntohs(packet.nQuestions);	}
	ushort getNAnswers()	{return ntohs(packet.nAnswers);		}
	ushort getNAdditional() {return ntohs(packet.nAdditional);	}
	ushort getNAuthority()	{return ntohs(packet.nAuthority);	}
	


	uint   getPacketSize();

	void setRD();
	void setQuestion(std::vector<std::string> host_parts, uint queryType);
	void setQueryType(uint queryType);
	void setQueryClass();

	void changePackets(struct UDP_Packet& npacket, int packet_len);

	void parsePacket();
	std::string processJump(ushort jump_addr, ushort &curr_ind);
	std::string parseSegment(ushort& ind);
	

	void printPacket();
};

#pragma pack(pop)
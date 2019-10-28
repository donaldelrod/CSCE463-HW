#include "pch.h"
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Packet.h"




Packet::Packet(ushort txid, std::vector<std::string> host_parts, uint queryType) {

	//sets the txid, nQuestions, nAnswers, nAuthority, and nAdditional
	packet.txid = htons(txid);
	packet.nQuestions = htons(1);
	packet.nAnswers = htons(0);
	packet.nAuthority = htons(0);
	packet.nAdditional = htons(0);

	//sets the flag to 0 and then sets the RD flag to 1
	packet.flags.flags = htons(0);
	setRD();
	packet.flags.flags = htons(packet.flags.flags);

	lookup_string = "";
	var_ind = 0;

	//constructs the question/type/class data portion
	setQuestion(host_parts, queryType);
	setQueryType(queryType);
	setQueryClass();
}


void Packet::setQuestion(std::vector<std::string> host_parts, uint queryType) {

	if (queryType == DNS_PTR) {
		host_parts.insert(host_parts.begin(), "in-addr");
		host_parts.insert(host_parts.begin(), "arpa");
		for (int i = host_parts.size() - 1; i >= 0; i--) {
			int part_size = host_parts.at(i).length();
			std::string part = host_parts.at(i);
			//this part puts the length of the first label in the query buffer
			packet.variable[var_ind] = part_size;
			var_ind++;
			//and here we copy the label into the buffer and increase the buffer index
			strncpy(packet.variable + var_ind, part.c_str(), part_size);
			var_ind += part_size;
			//and here we add the label to a string so it can be printed
			lookup_string += part + ".";
		}

		lookup_string += "in-addr.arpa";

	}
	else if (queryType == DNS_A) {
		//adds the parts to the buffer/string 
		for (int i = 0; i < host_parts.size(); i++) {
			int part_size = host_parts.at(i).length();
			std::string part = host_parts.at(i);
			//this part puts the length of the first label in the query buffer
			packet.variable[var_ind++] = part_size;
			//and here we copy the label into the buffer
			strncpy(packet.variable + var_ind, part.c_str(), part_size);
			var_ind += part_size;
			//and here we add the label to a string so it can be printed
			lookup_string += part;
			if (i != host_parts.size() - 1)
				lookup_string += ".";
		}
	}

	//here we add the last 0x00 byte to the buffer
	packet.variable[var_ind++] = 0x00;
}

void Packet::setQueryType(uint queryType) {
	//sets the two bytes to 0x0000 + queryType
	packet.variable[var_ind] = htons(0x00);
	var_ind++;
	packet.variable[var_ind] = queryType;
	var_ind++;
}

void Packet::setQueryClass() {
	//sets the two bytes to 0x0001, which is type IN
	packet.variable[var_ind++] = htons(0x00);
	packet.variable[var_ind++] = 0x01;
}

void Packet::setRD() {
	packet.flags.bits.rcr_d = 1;
}

void Packet::changePackets(struct UDP_Packet& npacket, int packet_len) {
	sent_packet = packet;
	packet = npacket;
	// -12 because thats the size of the UDP and DNS headers
	resp_ind = packet_len;
	parsePacket();
}

void Packet::parsePacket() {
	ushort curr_ind = var_ind;
	if (sent_packet.txid != packet.txid) {
		//the txids dont match, complain
		printf("\n\nthe txids do not match\n");
	}
	if (sent_packet.nQuestions != packet.nQuestions) {
		//number of questions dont match, complain
		printf("\n\nthe number of questions do not match\n");
	}
	packet.flags.flags = htons(packet.flags.flags);
	if (packet.flags.bits.query != 1) {
		//not valid response packet, complain
		printf("\n\nthe query bit is not set\n");
	}

	int ques_processed = 0;
	std::string question = "";
	int num_q = getNQuestions();
	while (ques_processed < num_q) {

	}


	int ans_processed = 0;
	std::string answer = "";

	//loop until a string for all questions have been grabbed
	int num_a = getNAnswers();
	ushort jump_addr = curr_ind;
	while (ans_processed < num_a) {
		//get the string
		
		answer = processJump(jump_addr, curr_ind);
		jump_addr += 2;

		//get type, class, and ttl
		ushort* type_p = (ushort*)(packet.variable + jump_addr);
		jump_addr += 2;
		ushort* class_p = (ushort*)(packet.variable + jump_addr);
		jump_addr += 2;
		uint* ttl_p = (uint*)(packet.variable + jump_addr);
		jump_addr += 4;
		ushort* data_len_p = (ushort*)(packet.variable + jump_addr);
		jump_addr += 2;

		ushort type_n = ntohs(*type_p);
		ushort class_n = ntohs(*class_p);
		uint ttl_n = ntohl(*ttl_p);
		ushort data_len_n = ntohs(*data_len_p);

		if (answer[answer.length() - 1] == '.') {
			answer.erase(answer.length() - 1);
		}
		if (type_n == DNS_A) {
			struct in_addr paddr;
			uint *ip = (uint*)(packet.variable + curr_ind);
			paddr.S_un.S_addr = htonl(*ip);
			curr_ind += 4;

			char *ip_char = inet_ntoa(paddr);
			answer += " A ";
			answer += std::string(ip_char);
			answer += " TTL = " + std::to_string(ttl_n);
			
		}
		else if (type_n == DNS_PTR) {
			answer += " PTR ";
			answer += "TTL = " + std::to_string(ttl_n);
		}
		else if (type_n == DNS_CNAME) {
			answer += " CNAME ";
			//jump_addr = curr_ind;
			std::string tans = processJump(jump_addr, curr_ind);
			if (tans[tans.length() - 1] == '.') {
				tans.erase(tans.length() - 1);
			}
			answer += tans;
			answer += " TTL = " + std::to_string(ttl_n);
		}
		else {
			ans_processed++;
			curr_ind += data_len_n;
			continue;
		}
		

		answers.push_back(answer);
		answer = "";
		ans_processed++;
	}

	
	
		
	//ushort jump = packet.variable[var_ind++];
	


}

std::string Packet::parseSegment(ushort& ind) {
	char len = packet.variable[ind++];
	ushort length = len;
	char* seg = (char*)malloc(length);
	memset(seg, 0, length);
	memcpy(seg, packet.variable + ind, length);
	ind += length;
	return std::string(seg, length);
}

std::string Packet::processJump(ushort jump_addr, ushort &curr_ind) {
	std::string part = "";
	uchar curr_char;
	//get the jump if there is one
	while (( curr_char = packet.variable[jump_addr]) != 0x00) {
		char* curr_addr = packet.variable + jump_addr;
		ushort* j = (ushort*)curr_addr;
		struct compressed_jump compressed;
		compressed.num = *j;
		//struct compressed_jump *compressed = (struct compressed_jump*)curr_addr;
		compressed.num = ntohs(compressed.num);
		if (compressed.upper2 == 0x3) { //if compressed response
			part += processJump(compressed.lower14 - 12, curr_ind);
			jump_addr = curr_ind;
		}
		else { //otherwise parse it like any other
			part += parseSegment(jump_addr) + ".";
			//jump_addr = curr_ind;
		}
	}
	curr_ind = jump_addr;
	return part;
}

void Packet::printPacket() {
	std::cout << "DNS Packet Contents:\n";
	std::string l = "---------------------------------\n";

	std::cout << l << std::endl;
	printf("DNS Data:\n");
	std::cout << l;
	printf("|\t%#x\t|\t%#x\t|\t|   TX ID    |     FLAGS   |\n",	packet.txid,		packet.flags.flags);
	std::cout << l;
	printf("|\t%#x\t|\t%#x\t|\t| nQuestions |   nAnswers  |\n",	packet.nQuestions,	packet.nAnswers);
	std::cout << l;
	printf("|\t%#x\t|\t%#x\t|\t| nAuthority | nAdditional |\n", packet.nAuthority,	packet.nAdditional);
	std::cout << l;

	char *buf = new char[var_ind];
	printf("Question length: %d\n", var_ind);
	strncpy(buf, packet.variable, var_ind);
	printf("Question data: %s\n", buf);

}

uint Packet::getPacketSize() {
	//20 is the size of the UDP header and the DNS header, and var_ind is the
	//amount of data in the buffer section
	return 20 + var_ind;
}
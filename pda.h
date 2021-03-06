

#ifndef PDA_H_
#define PDA_H_


#include "aqualink.h"
#include "config.h"

void init_pda(struct aqualinkdata *aqdata, struct aqconfig *aqconf);

bool process_pda_packet(unsigned char* packet, int length);
bool pda_shouldSleep();

#endif // PDA_MESSAGES_H_

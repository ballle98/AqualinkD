
#ifndef PDA_AQ_PROGRAMMER_H_
#define PDA_AQ_PROGRAMMER_H_

typedef enum pda_type {
  AQUAPALM,
  PDA
} pda_type;

void *get_aqualink_PDA_device_status( void *ptr );
void *set_aqualink_PDA_device_on_off( void *ptr );
void *set_aqualink_PDA_wakeinit( void *ptr );
void *set_aqualink_PDA_init( void *ptr );

bool set_PDA_aqualink_heater_setpoint(struct aqualinkdata *aq_data, int val, bool isPool);
bool set_PDA_aqualink_SWG_setpoint(struct aqualinkdata *aq_data, int val);
bool set_PDA_aqualink_freezeprotect_setpoint(struct aqualinkdata *aq_data, int val);

bool get_PDA_aqualink_pool_spa_heater_temps(struct aqualinkdata *aq_data);
bool get_PDA_freeze_protect_temp(struct aqualinkdata *aq_data);

bool get_PDA_aqualink_aux_labels(struct aqualinkdata *aq_data);

bool set_PDA_aqualink_boost(struct aqualinkdata *aq_data, bool val);
bool set_PDA_aqualink_time(struct aqualinkdata *aq_data);


// These are from aq_programmer.c , exposed here for PDA AQ PROGRAMMER
bool send_cmd(unsigned char cmd);
bool push_aq_cmd(unsigned char cmd);
bool waitForMessage(struct aqualinkdata *aq_data, char* message, int numMessageReceived);
void waitfor_queue2empty();
void longwaitfor_queue2empty();

//void pda_programming_thread_check(struct aqualinkdata *aq_data);

#endif // AQ_PDA_PROGRAMMER_H_

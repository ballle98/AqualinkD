
/*
 * Copyright (c) 2017 Shaun Feakes - All rights reserved
 *
 * You may use redistribute and/or modify this code under the terms of
 * the GNU General Public License version 2 as published by the 
 * Free Software Foundation. For the terms of this license, 
 * see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 *  https://github.com/sfeakes/aqualinkd
 */

#define _GNU_SOURCE 1 // for strcasestr & strptime
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "aqualink.h"

#include "pda.h"
#include "pda_menu.h"
#include "utils.h"
#include "aq_panel.h"
#include "packetLogger.h"
#include "devices_jandy.h"
#include "rs_msg_utils.h"
#include "timespec_subtract.h"

// static struct aqualinkdata _aqualink_data;
static struct aqualinkdata *_aqualink_data = NULL;
static struct aqconfig *_config_parameters = NULL;
static unsigned char _last_packet_type;
static bool _initWithRS = false;
static bool _pda_first_probe_recvd = false;

#define PDA_SLEEP_FOR 30 // 30 seconds



void init_pda(struct aqualinkdata *aqdata, struct aqconfig *aqconf)
{
  _aqualink_data = aqdata;
  _config_parameters = aqconf;
  //set_pda_mode(true);
}


bool pda_shouldSleep() {
  //LOG(PDA_LOG,LOG_DEBUG, "PDA loop count %d, will sleep at %d\n",_pda_loop_cnt,PDA_LOOP_COUNT);
  struct timespec now;
  struct timespec elapsed;
  // If aqualinkd was restarted and a probe has not been received force a sleep
  if (! _pda_first_probe_recvd) {
    return true;
  } else if (! _config_parameters->pda_sleep_mode) {
    return false;
  }

  // NSF NEED TO CHECK ACTIVE THREADS.
  if (_aqualink_data->active_thread.thread_id != 0) {
    LOG(PDA_LOG,LOG_DEBUG, "PDA can't sleep as thread %d,%p is active\n",
               _aqualink_data->active_thread.ptype,
               _aqualink_data->active_thread.thread_id);
    return false;
  }

  // Last see if there are any open websockets. (don't sleep if the web UI is open)
  if ( _aqualink_data->open_websockets > 0 ) {
    LOG(PDA_LOG,LOG_DEBUG, "PDA can't sleep as websocket is active\n");
    return false;
  }

  clock_gettime(CLOCK_REALTIME, &now);
  timespec_subtract(&elapsed, &now, &(_aqualink_data->last_active_time));
  if (elapsed.tv_sec > PDA_SLEEP_FOR) {
    return false;
  }

  return true;
}

/*
bool pda_shouldSleep() {
  //LOG(PDA_LOG,LOG_DEBUG, "PDA loop count %d, will sleep at %d\n",_pda_loop_cnt,PDA_LOOP_COUNT);
  if (_pda_loop_cnt++ < PDA_LOOP_COUNT) {
    return false;
  } else if (_pda_loop_cnt > PDA_LOOP_COUNT*2) {
    _pda_loop_cnt = 0;
    return false;
  }

  return true;
}
*/

unsigned char get_last_pda_packet_type()
{
  return _last_packet_type;
}

void set_pda_led(struct aqualinkled *led, char state)
{
  aqledstate old_state = led->state;
  if (state == 'N')
  {
    led->state = ON;
  }
  else if (state == 'A')
  {
    led->state = ENABLE;
  }
  else if (state == '*')
  {
    led->state = FLASH;
  }
  else
  {
    led->state = OFF;
  }
  if (old_state != led->state)
  {
    LOG(PDA_LOG,LOG_DEBUG, "set_pda_led from %d to %d\n", old_state, led->state);
  }
}

void equiptment_update_cycle(int eqID) {
  // If you have a -1, it's a reset to clear / update information.
  int i;
  static uint32_t update_equiptment_bitmask = 0;

  if (eqID == -1) {
    LOG(PDA_LOG,LOG_DEBUG, "Start new equiptment cycle\n");
   
    for (i=0; i < _aqualink_data->total_buttons - 2 ; i++) { // total_buttons - 2 because we don't get heaters in this cycle
      if ((update_equiptment_bitmask & (1 << (i+1))) != (1 << (i+1))) {
        if (_aqualink_data->aqbuttons[i].led->state != OFF) {
          _aqualink_data->aqbuttons[i].led->state = OFF;
          _aqualink_data->updated = true;
          LOG(PDA_LOG,LOG_DEBUG, "Turn off equiptment id %d %s not seen in last cycle\n", i, _aqualink_data->aqbuttons[i].name);
        }
      }
    }
    update_equiptment_bitmask = 0;
  } else {
    update_equiptment_bitmask |= (1 << (eqID+1));
    LOG(PDA_LOG,LOG_DEBUG, "Added equiptment id %d %s to updated cycle\n", eqID, _aqualink_data->aqbuttons[eqID].name);
  }
}


void pass_pda_equiptment_status_item_OLD(char *msg)
{
  static char *index;
  int i;

  // EQUIPMENT STATUS
  //
  //  AquaPure 100%
  //  SALT 25500 PPM
  //   FILTER PUMP
  //    POOL HEAT
  //   SPA HEAT ENA

  // EQUIPMENT STATUS
  //
  //  FREEZE PROTECT
  //  AquaPure 100%
  //  SALT 25500 PPM
  //  CHECK AquaPure
  //  GENERAL FAULT
  //   FILTER PUMP
  //     CLEANER
  //
  // Equipment Status
  // 
  // Intelliflo VS 1 
  //      RPM: 1700  
  //     Watts: 367  
  // 
  // 
  // 
  // 
  // 

  // Check message for status of device
  // Loop through all buttons and match the PDA text.
  if ((index = strcasestr(msg, "CHECK AquaPure")) != NULL)
  {
    LOG(PDA_LOG,LOG_DEBUG, "CHECK AquaPure\n");
  }
  else if ((index = strcasestr(msg, "FREEZE PROTECT")) != NULL)
  {
    _aqualink_data->frz_protect_state = ON;
  }
  else if ((index = strcasestr(msg, MSG_SWG_PCT)) != NULL)
  {
    changeSWGpercent(_aqualink_data, atoi(index + strlen(MSG_SWG_PCT)));
    //_aqualink_data->swg_percent = atoi(index + strlen(MSG_SWG_PCT));
    //if (_aqualink_data->ar_swg_status == SWG_STATUS_OFF) {_aqualink_data->ar_swg_status = SWG_STATUS_ON;}
    LOG(PDA_LOG,LOG_DEBUG, "AquaPure = %d\n", _aqualink_data->swg_percent);
  }
  else if ((index = strcasestr(msg, MSG_SWG_PPM)) != NULL)
  {
    _aqualink_data->swg_ppm = atoi(index + strlen(MSG_SWG_PPM));
    //if (_aqualink_data->ar_swg_status == SWG_STATUS_OFF) {_aqualink_data->ar_swg_status = SWG_STATUS_ON;}
    LOG(PDA_LOG,LOG_DEBUG, "SALT = %d\n", _aqualink_data->swg_ppm);
  }
  else if ((index = strcasestr(msg, MSG_PMP_RPM)) != NULL)
  { // Default to pump 0, should check for correct pump
    _aqualink_data->pumps[0].rpm = atoi(index + strlen(MSG_PMP_RPM));
    LOG(PDA_LOG,LOG_DEBUG, "RPM = %d\n", _aqualink_data->pumps[0].rpm);
  }
  else if ((index = strcasestr(msg, MSG_PMP_WAT)) != NULL)
  { // Default to pump 0, should check for correct pump
    _aqualink_data->pumps[0].watts = atoi(index + strlen(MSG_PMP_WAT));
    LOG(PDA_LOG,LOG_DEBUG, "Watts = %d\n", _aqualink_data->pumps[0].watts);
  }
  else
  {
    char labelBuff[AQ_MSGLEN + 2];
    strncpy(labelBuff, msg, AQ_MSGLEN + 1);
    msg = stripwhitespace(labelBuff);

    if (strcasecmp(msg, "POOL HEAT ENA") == 0)
    {
      _aqualink_data->aqbuttons[_aqualink_data->pool_heater_index].led->state = ENABLE;
    }
    else if (strcasecmp(msg, "SPA HEAT ENA") == 0)
    {
      _aqualink_data->aqbuttons[_aqualink_data->spa_heater_index].led->state = ENABLE;
    }
    else
    {
      for (i = 0; i < _aqualink_data->total_buttons; i++)
      {
        if (strcasecmp(msg, _aqualink_data->aqbuttons[i].label) == 0)
        {
          equiptment_update_cycle(i);
          LOG(PDA_LOG,LOG_DEBUG, "Status for %s = '%.*s'\n", _aqualink_data->aqbuttons[i].label, AQ_MSGLEN, msg);
          // It's on (or delayed) if it's listed here.
          if (_aqualink_data->aqbuttons[i].led->state != FLASH)
          {
            _aqualink_data->aqbuttons[i].led->state = ON;
          }
          break;
        }
      }
    }
  }
}

void process_pda_packet_msg_long_temp(const char *msg)
{
  //           'AIR         POOL'
  //           ' 86`     86`    '
  //           'AIR   SPA       '
  //           ' 86` 86`        '
  //           'AIR             '
  //           ' 86`            '
  //           'AIR        WATER'  // In case of single device.
  _aqualink_data->temp_units = FAHRENHEIT; // Force FAHRENHEIT
  if (stristr(pda_m_line(1), "AIR") != NULL)
    _aqualink_data->air_temp = atoi(msg);

  if (stristr(pda_m_line(1), "SPA") != NULL)
  {
    _aqualink_data->spa_temp = atoi(msg + 4);
    _aqualink_data->pool_temp = TEMP_UNKNOWN;
  }
  else if (stristr(pda_m_line(1), "POOL") != NULL)
  {
    _aqualink_data->pool_temp = atoi(msg + 7);
    _aqualink_data->spa_temp = TEMP_UNKNOWN;
  }
  else if (stristr(pda_m_line(1), "WATER") != NULL)
  {
    _aqualink_data->pool_temp = atoi(msg + 7);
    _aqualink_data->spa_temp = TEMP_UNKNOWN;
  }
  else
  {
    _aqualink_data->pool_temp = TEMP_UNKNOWN;
    _aqualink_data->spa_temp = TEMP_UNKNOWN;
  }
  // printf("Air Temp = %d | Water Temp = %d\n",atoi(msg),atoi(msg+7));
}

void process_pda_packet_msg_long_time(const char *msg)
{
  // message "     SAT 8:46AM "
  //         "     SAT 10:29AM"
  //         "     SAT 4:23PM "
  //         "     SUN  2:36PM"
  // printf("TIME = '%.*s'\n",AQ_MSGLEN,msg );
  // printf("TIME = '%c'\n",msg[AQ_MSGLEN-1] );

  if (msg[AQ_MSGLEN - 1] == ' ')
  {
    snprintf(_aqualink_data->time, sizeof(_aqualink_data->time), "%.6s", msg + 9);
  }
  else
  {
    snprintf(_aqualink_data->time, sizeof(_aqualink_data->time), "%.7s", msg + 9);
  }
  snprintf(_aqualink_data->date, sizeof(_aqualink_data->date), "%.3s", msg + 5);

  if (checkAqualinkTime() != true)
  {
    LOG(AQRS_LOG,LOG_NOTICE, "RS time is NOT accurate '%s %s', re-setting on controller!\n", _aqualink_data->time, _aqualink_data->date);
    aq_programmer(AQ_SET_TIME, NULL, _aqualink_data);
  } 
}

void process_pda_packet_msg_long_equipment_control(const char *msg)
{
  // These are listed as "FILTER PUMP  OFF"
  int i;
  char labelBuff[AQ_MSGLEN + 1];
  strncpy(labelBuff, msg, AQ_MSGLEN - 4);
  labelBuff[AQ_MSGLEN - 4] = 0;

  LOG(PDA_LOG,LOG_DEBUG, "*** Checking Equiptment '%s'\n", labelBuff);

  for (i = 0; i < _aqualink_data->total_buttons; i++)
  {
    if (strcasecmp(stripwhitespace(labelBuff), _aqualink_data->aqbuttons[i].label) == 0)
    {
      LOG(PDA_LOG,LOG_DEBUG, "*** Found EQ CTL Status for %s = '%.*s'\n", _aqualink_data->aqbuttons[i].label, AQ_MSGLEN, msg);
      set_pda_led(_aqualink_data->aqbuttons[i].led, msg[AQ_MSGLEN - 1]);
    }
  }

  // Force SWG off if pump is off.
  if (_aqualink_data->aqbuttons[0].led->state == OFF )
    setSWGoff(_aqualink_data);
    //_aqualink_data->ar_swg_status = SWG_STATUS_OFF;

  // NSF I think we need to check TEMP1 and TEMP2 and set Pool HEater and Spa heater directly, to support single device.
  if (isSINGLE_DEV_PANEL){
    if (strcasecmp(stripwhitespace(labelBuff), "TEMP1") == 0)
      set_pda_led(_aqualink_data->aqbuttons[_aqualink_data->pool_heater_index].led, msg[AQ_MSGLEN - 1]);
    if (strcasecmp(stripwhitespace(labelBuff), "TEMP2") == 0)
      set_pda_led(_aqualink_data->aqbuttons[_aqualink_data->spa_heater_index].led, msg[AQ_MSGLEN - 1]);
  }
}

void process_pda_packet_msg_long_home(const char *msg)
{
  if (stristr(msg, "POOL MODE") != NULL)
  {
    // If pool mode is on the filter pump is on but if it is off the filter pump might be on if spa mode is on.
    if (msg[AQ_MSGLEN - 1] == 'N')
    {
      _aqualink_data->aqbuttons[PUMP_INDEX].led->state = ON;
    }
    else if (msg[AQ_MSGLEN - 1] == '*')
    {
      _aqualink_data->aqbuttons[PUMP_INDEX].led->state = FLASH;
    }
  }
  else if (stristr(msg, "POOL HEATER") != NULL)
  {
    set_pda_led(_aqualink_data->aqbuttons[_aqualink_data->pool_heater_index].led, msg[AQ_MSGLEN - 1]);
  }
  else if (stristr(msg, "SPA MODE") != NULL)
  {
    // when SPA mode is on the filter may be on or pending
    if (msg[AQ_MSGLEN - 1] == 'N')
    {
      _aqualink_data->aqbuttons[PUMP_INDEX].led->state = ON;
      _aqualink_data->aqbuttons[SPA_INDEX].led->state = ON;
    }
    else if (msg[AQ_MSGLEN - 1] == '*')
    {
      _aqualink_data->aqbuttons[PUMP_INDEX].led->state = FLASH;
      _aqualink_data->aqbuttons[SPA_INDEX].led->state = ON;
    }
    else
    {
      _aqualink_data->aqbuttons[SPA_INDEX].led->state = OFF;
    }
  }
  else if (stristr(msg, "SPA HEATER") != NULL)
  {
    set_pda_led(_aqualink_data->aqbuttons[_aqualink_data->spa_heater_index].led, msg[AQ_MSGLEN - 1]);
  }
}

void setSingleDeviceMode()
{
  if (isSINGLE_DEV_PANEL != true)
  {
    changePanelToMode_Only();
    LOG(AQRS_LOG,LOG_ERR, "AqualinkD set to 'Combo Pool & Spa' but detected 'Only Pool OR Spa' panel, please change config\n");
  }
}

void process_pda_packet_msg_long_set_time(const char *msg)
{
/*
  // NOT Working at moment, also wrong format
  LOG(PDA_LOG,LOG_DEBUG, "process_pda_packet_msg_long_set_temp\n");
  if (msg[4] == '/' && msg[7] == '/'){
    //DATE
    //rsm_strncpycut(_aqualink_data->date, msg, AQ_MSGLEN-1, AQ_MSGLEN-1);
    strncpy(_aqualink_data->date, msg + 11, 3);
  } else if (msg[6] == ':' && msg[11] == 'M') {
    // TIME
    //rsm_strncpycut(_aqualink_data->time, msg, AQ_MSGLEN-1, AQ_MSGLEN-1);
    if (msg[4] == ' ')
      strncpy(_aqualink_data->time, msg + 5, 6);
    else
  }
 */ 
}

void process_pda_packet_msg_long_set_temp(const char *msg)
{
  LOG(PDA_LOG,LOG_DEBUG, "process_pda_packet_msg_long_set_temp\n");

  if (stristr(msg, "POOL HEAT") != NULL)
  {
    _aqualink_data->pool_htr_set_point = atoi(msg + 10);
    LOG(PDA_LOG,LOG_DEBUG, "pool_htr_set_point = %d\n", _aqualink_data->pool_htr_set_point);
  }
  else if (stristr(msg, "SPA HEAT") != NULL)
  {
    _aqualink_data->spa_htr_set_point = atoi(msg + 10);
    LOG(PDA_LOG,LOG_DEBUG, "spa_htr_set_point = %d\n", _aqualink_data->spa_htr_set_point);
  }
  else if (stristr(msg, "TEMP1") != NULL)
  {
    setSingleDeviceMode();
    _aqualink_data->pool_htr_set_point = atoi(msg + 10);
    LOG(PDA_LOG,LOG_DEBUG, "pool_htr_set_point = %d\n", _aqualink_data->pool_htr_set_point);
  }
  else if (stristr(msg, "TEMP2") != NULL)
  {
    setSingleDeviceMode();
    _aqualink_data->spa_htr_set_point = atoi(msg + 10);
    LOG(PDA_LOG,LOG_DEBUG, "spa_htr_set_point = %d\n", _aqualink_data->spa_htr_set_point);
  }

 
}

void process_pda_packet_msg_long_spa_heat(const char *msg)
{
  if (strncasecmp(msg, "    ENABLED     ", 16) == 0)
  {
    _aqualink_data->aqbuttons[_aqualink_data->spa_heater_index].led->state = ENABLE;
  }
  else if (strncasecmp(msg, "  SET TO", 8) == 0)
  {
    _aqualink_data->spa_htr_set_point = atoi(msg + 8);
    LOG(PDA_LOG,LOG_DEBUG, "spa_htr_set_point = %d\n", _aqualink_data->spa_htr_set_point);
  }
}

void process_pda_packet_msg_long_pool_heat(const char *msg)
{
  if (strncasecmp(msg, "    ENABLED     ", 16) == 0)
  {
    _aqualink_data->aqbuttons[_aqualink_data->pool_heater_index].led->state = ENABLE;
  }
  else if (strncasecmp(msg, "  SET TO", 8) == 0)
  {
    _aqualink_data->pool_htr_set_point = atoi(msg + 8);
    LOG(PDA_LOG,LOG_DEBUG, "pool_htr_set_point = %d\n", _aqualink_data->pool_htr_set_point);
  }
}

void process_pda_packet_msg_long_freeze_protect(const char *msg)
{
  if (strncasecmp(msg, "TEMP      ", 10) == 0)
  {
    _aqualink_data->frz_protect_set_point = atoi(msg + 10);
    LOG(PDA_LOG,LOG_DEBUG, "frz_protect_set_point = %d\n", _aqualink_data->frz_protect_set_point);
  }
}

void process_pda_packet_msg_long_SWG(const char *msg)
{
  //PDA Line 0 =   SET AquaPure
  //PDA Line 1 =
  //PDA Line 2 =
  //PDA Line 3 = SET POOL TO: 45%
  //PDA Line 4 =  SET SPA TO:  0%

  // If spa is on, read SWG for spa, if not set SWG for pool
  if (_aqualink_data->aqbuttons[SPA_INDEX].led->state != OFF) {
    if (strncasecmp(msg, "SET SPA TO:", 11) == 0)
    {
      //_aqualink_data->swg_percent = atoi(msg + 13);
      setSWGpercent(_aqualink_data, atoi(msg + 13));
      LOG(PDA_LOG,LOG_DEBUG, "SPA swg_percent = %d\n", _aqualink_data->swg_percent);
    }
  } else {
    if (strncasecmp(msg, "SET POOL TO:", 12) == 0)
    {
      //_aqualink_data->swg_percent = atoi(msg + 13);
      setSWGpercent(_aqualink_data, atoi(msg + 13));
      LOG(PDA_LOG,LOG_DEBUG, "POOL swg_percent = %d\n", _aqualink_data->swg_percent);
    } 
  }
}

void process_pda_packet_msg_long_unknown(const char *msg)
{
  int i;
  // Lets make a guess here and just see if there is an ON/OFF/ENA/*** at the end of the line
  // When you turn on/off a piece of equiptment, a clear screen followed by single message is sent.
  // So we are not in any PDA menu, try to catch that message here so we catch new device state ASAP.
  if (msg[AQ_MSGLEN - 1] == 'N' || msg[AQ_MSGLEN - 1] == 'F' || msg[AQ_MSGLEN - 1] == 'A' || msg[AQ_MSGLEN - 1] == '*')
  {
    for (i = 0; i < _aqualink_data->total_buttons; i++)
    {
      if (stristr(msg, _aqualink_data->aqbuttons[i].label) != NULL)
      {
        //LOG(PDA_LOG,LOG_ERR," UNKNOWN Found Status for %s = '%.*s'\n", _aqualink_data->aqbuttons[i].label, AQ_MSGLEN, msg);
        // This seems to keep everything off.
        set_pda_led(_aqualink_data->aqbuttons[i].led, msg[AQ_MSGLEN-1]);
      }
    }
  }
}

void pda_pump_update(struct aqualinkdata *aq_data, int updated) {
  const int bitmask[MAX_PUMPS] = {1,2,4,8};
  static unsigned char updates = '\0';
  int i;

  if (updated == -1) {
    for(i=0; i < MAX_PUMPS; i++) {
      if ((updates & bitmask[i]) != bitmask[i]) {
        aq_data->pumps[i].rpm = PUMP_OFF_RPM;
        aq_data->pumps[i].gpm = PUMP_OFF_GPM;
        aq_data->pumps[i].watts = PUMP_OFF_WAT;
      }
    }
    updates = '\0';
  } else if (updated >=0 && updated < MAX_PUMPS) {
     updates |= bitmask[updated];
  }
}

void log_pump_information() {
  int i;
  //bool rtn = false;
  char *m2_line=pda_m_line(2);
  char *m3_line=pda_m_line(3);
  char *m4_line=pda_m_line(3);
  char *m5_line=pda_m_line(3);

  if (rsm_strcmp(m2_line,"Intelliflo VS") == 0 ||
      rsm_strcmp(m2_line,"Intelliflo VF") == 0 ||
      rsm_strcmp(m2_line,"Jandy ePUMP") == 0 ||
      rsm_strcmp(m2_line,"ePump AC") == 0) {
    //rtn = true;
    int rpm = 0;
    int watts = 0;
    int gpm = 0;
    int pump_index = rsm_atoi(&m2_line[14]);
    if (pump_index <= 0)
      pump_index = rsm_atoi(&m2_line[12]); // Pump inxed is in different position on line `  ePump AC  4`
    // RPM displays differently depending on 3 or 4 digit rpm.
    if (rsm_strcmp(m3_line,"RPM:") == 0){
      rpm = rsm_atoi(&m3_line[10]);
      if (rsm_strcmp(m4_line,"Watts:") == 0) {
        watts = rsm_atoi(&m4_line[10]);
      }
      if (rsm_strcmp(m5_line,"GPM:") == 0) {
        gpm = rsm_atoi(&m5_line[10]);
      }
    } else if (rsm_strcmp(m3_line,"*** Priming ***") == 0){
      rpm = PUMP_PRIMING;
    } else if (rsm_strcmp(m3_line,"(Offline)") == 0){
      rpm = PUMP_OFFLINE;
    } else if (rsm_strcmp(m3_line,"(Priming Error)") == 0){
      rpm = PUMP_ERROR;
    }

    LOG(PDA_LOG,LOG_INFO, "PDA Pump %s, Index %d, RPM %d, Watts %d, GPM %d\n",m3_line,pump_index,rpm,watts,gpm);

    for (i=0; i < _aqualink_data->num_pumps; i++) {
      if (_aqualink_data->pumps[i].pumpIndex == pump_index) {
        LOG(PDA_LOG,LOG_INFO, "PDA Pump label: %s Index: %d, ID: %d, RPM: %d, Watts: %d, GPM: %d\n",_aqualink_data->pumps[i].button->name, i ,pump_index,pump_index,rpm,watts,gpm);
        //printf("**** FOUND PUMP %d at index %d *****\n",pump_index,i);
        //aq_data->pumps[i].updated = true;
        pda_pump_update(_aqualink_data, i);
        _aqualink_data->pumps[i].rpm = rpm;
        _aqualink_data->pumps[i].watts = watts;
        _aqualink_data->pumps[i].gpm = gpm;
        if (_aqualink_data->pumps[i].pumpType == PT_UNKNOWN){
          if (rsm_strcmp(m2_line,"Intelliflo VS") == 0)
            _aqualink_data->pumps[i].pumpType = VSPUMP;
          else if (rsm_strcmp(m2_line,"Intelliflo VF") == 0)
            _aqualink_data->pumps[i].pumpType = VFPUMP;
          else if (rsm_strcmp(m2_line,"Jandy ePUMP") == 0 ||
                   rsm_strcmp(m2_line,"ePump AC") == 0)
            _aqualink_data->pumps[i].pumpType = EPUMP;
        }
        //printf ("Set Pump Type to %d\n",aq_data->pumps[i].pumpType);
        return;
      }
    }
    LOG(PDA_LOG,LOG_ERR, "PDA Could not find config for Pump %s, Index %d, RPM %d, Watts %d, GPM %d\n",m3_line,pump_index,rpm,watts,gpm);
  }
}

void process_pda_packet_msg_long_equiptment_status(const char *msg_line, int lineindex, bool reset)
{
  LOG(PDA_LOG,LOG_DEBUG, "*** Pass Equiptment msg '%.16s'\n", msg_line);

  if (msg_line == NULL) {
    LOG(PDA_LOG,LOG_DEBUG, "*** Pass Equiptment msg is NULL do nothing\n");
    return;
  }

  static char *index;
  int i;
  char *msg = (char *)msg_line;
  while(isspace(*msg)) msg++;
  
    //strncpy(labelBuff, msg, AQ_MSGLEN);

  // EQUIPMENT STATUS
  //
  //  AquaPure 100%
  //  SALT 25500 PPM
  //   FILTER PUMP
  //    POOL HEAT
  //   SPA HEAT ENA

  // EQUIPMENT STATUS
  //
  //  FREEZE PROTECT
  //  AquaPure 100%
  //  SALT 25500 PPM
  //  CHECK AquaPure
  //  GENERAL FAULT
  //   FILTER PUMP
  //     CLEANER
  //

  // VSP Pumps are not read here, since they are over multiple lines.

  // Check message for status of device
  // Loop through all buttons and match the PDA text.
  // Should probably use strncasestr
  if ((index = rsm_strnstr(msg, "CHECK AquaPure", AQ_MSGLEN)) != NULL)
  {
    LOG(PDA_LOG,LOG_DEBUG, "CHECK AquaPure\n");
  }
  else if ((index = rsm_strnstr(msg, "FREEZE PROTECT", AQ_MSGLEN)) != NULL)
  {
    _aqualink_data->frz_protect_state = ON;
    LOG(PDA_LOG,LOG_DEBUG, "Freeze Protect is on\n");
  }
  else if ((index = rsm_strnstr(msg, MSG_SWG_PCT, AQ_MSGLEN)) != NULL)
  {
    changeSWGpercent(_aqualink_data, atoi(index + strlen(MSG_SWG_PCT)));
    //_aqualink_data->swg_percent = atoi(index + strlen(MSG_SWG_PCT));
    //if (_aqualink_data->ar_swg_status == SWG_STATUS_OFF) {_aqualink_data->ar_swg_status = SWG_STATUS_ON;}
    LOG(PDA_LOG,LOG_DEBUG, "AquaPure = %d\n", _aqualink_data->swg_percent);
  }
  else if ((index = rsm_strnstr(msg, MSG_SWG_PPM, AQ_MSGLEN)) != NULL)
  {
    _aqualink_data->swg_ppm = atoi(index + strlen(MSG_SWG_PPM));
    //if (_aqualink_data->ar_swg_status == SWG_STATUS_OFF) {_aqualink_data->ar_swg_status = SWG_STATUS_ON;}
    LOG(PDA_LOG,LOG_DEBUG, "SALT = %d\n", _aqualink_data->swg_ppm);
  }  
  else if (rsm_strncmp(msg_line, "POOL HEAT ENA",AQ_MSGLEN) == 0)
  {
      _aqualink_data->aqbuttons[_aqualink_data->pool_heater_index].led->state = ENABLE;
      LOG(PDA_LOG,LOG_DEBUG, "Pool Hearter is enabled\n");
      //equiptment_update_cycle(_aqualink_data->pool_heater_index);
  }
  else if (rsm_strncmp(msg_line, "SPA HEAT ENA",AQ_MSGLEN) == 0)
  {
      _aqualink_data->aqbuttons[_aqualink_data->spa_heater_index].led->state = ENABLE;
      LOG(PDA_LOG,LOG_DEBUG, "Spa Hearter is enabled\n");
      //equiptment_update_cycle(_aqualink_data->spa_heater_index);
  }
  else
  {
      for (i = 0; i < _aqualink_data->total_buttons; i++)
      {
        //LOG(PDA_LOG,LOG_DEBUG, "*** check msg '%s' against '%s'\n",labelBuff,_aqualink_data->aqbuttons[i].label);
        //LOG(PDA_LOG,LOG_DEBUG, "*** check msg '%.*s' against '%s'\n",AQ_MSGLEN,msg_line,_aqualink_data->aqbuttons[i].label);
        if (rsm_strncmp(msg_line, _aqualink_data->aqbuttons[i].label, AQ_MSGLEN-1) == 0)
        //if (rsm_strcmp(_aqualink_data->aqbuttons[i].label, labelBuff) == 0)
        {
          equiptment_update_cycle(i);
          LOG(PDA_LOG,LOG_DEBUG, "Found Status for %s = '%.*s'\n", _aqualink_data->aqbuttons[i].label, AQ_MSGLEN, msg_line);
          // It's on (or delayed) if it's listed here.
          if (_aqualink_data->aqbuttons[i].led->state != FLASH)
          {
            _aqualink_data->aqbuttons[i].led->state = ON;
          }
          break;
        }
      }
  }
  


}

void process_pda_packet_msg_long_level_aux_device(const char *msg)
{
#ifdef BETA_PDA_AUTOLABEL
  int li=-1;
  char *str, *label;

  if (! _aqconfig_->use_panel_aux_labels)
    return;
  // NSF  Need to check config for use_panel_aux_labels value and ignore if not set

  // Only care once we have the full menu, so check line 
  //  PDA Line 0 =    LABEL AUX1   
  //  PDA Line 1 =     
  //  PDA Line 2 =   CURRENT LABEL 
  //  PDA Line 3 =       AUX1          


  if ( (strlen(pda_m_line(3)) > 0 ) &&  
     (strncasecmp(pda_m_line(2),"  CURRENT LABEL ", 16) == 0) && 
     (strncasecmp(pda_m_line(0),"   LABEL AUX", 12) == 0)                           ) {
    str = pda_m_line(0);
    li = atoi(&str[12] );  // 12 is a guess, need to check on real system
    if (li > 0) {
      str = cleanwhitespace(pda_m_line(3));
      label = (char*)malloc(strlen(str)+1);
      strcpy ( label, str );
      _aqualink_data->aqbuttons[li-1].label = label;
    } else {
      LOG(PDA_LOG,LOG_ERR, "PDA couldn't get AUX? number\n", pda_m_line(0));
    }
  }
#endif
}

void process_pda_freeze_protect_devices()
{
  //  PDA Line 0 =  FREEZE PROTECT
  //  PDA Line 1 =     DEVICES
  //  PDA Line 2 =
  //  PDA Line 3 = FILTER PUMP    X
  //  PDA Line 4 = SPA
  //  PDA Line 5 = CLEANER        X
  //  PDA Line 6 = POOL LIGHT
  //  PDA Line 7 = SPA LIGHT
  //  PDA Line 8 = EXTRA AUX
  //  PDA Line 9 =
  int i;
  LOG(PDA_LOG,LOG_DEBUG, "process_pda_freeze_protect_devices\n");
  for (i = 1; i < PDA_LINES; i++)
  {
    if (pda_m_line(i)[AQ_MSGLEN - 1] == 'X')
    {
      LOG(PDA_LOG,LOG_DEBUG, "PDA freeze protect enabled by %s\n", pda_m_line(i));
      if (_aqualink_data->frz_protect_state == OFF)
      {
        _aqualink_data->frz_protect_state = ENABLE;
        break;
      }
    }
  }
}

bool process_pda_packet(unsigned char *packet, int length)
{
  bool rtn = true;
  //int i;
  char *msg;
  int index = -1;
  static bool equiptment_update_loop = false;
  static bool read_equiptment_menu = false;


  process_pda_menu_packet(packet, length, in_programming_mode(_aqualink_data));


  switch (packet[PKT_CMD])
  {

  case CMD_PROBE:
    _pda_first_probe_recvd = true;
    break;
  case CMD_ACK:
    LOG(PDA_LOG,LOG_DEBUG, "RS Received ACK length %d.\n", length);
    break;

    case CMD_PDA_CLEAR:
      read_equiptment_menu = false; // Reset the have read menu flag, since this is new menu.
    break;

    case CMD_STATUS:
      _aqualink_data->last_display_message[0] = '\0';
      if (equiptment_update_loop == false && pda_m_type() == PM_EQUIPTMENT_STATUS)
      {
        LOG(PDA_LOG,LOG_DEBUG, "**** PDA Start new Equiptment loop ****\n");
        equiptment_update_loop = true;
        // Need to process the equiptment full MENU here
      }
      else if (read_equiptment_menu == false && equiptment_update_loop == true && pda_m_type() == PM_EQUIPTMENT_STATUS)
      {
        LOG(PDA_LOG,LOG_DEBUG, "**** PDA read Equiptment page ****\n");
        log_pump_information();
        read_equiptment_menu = true;
      }
      else if (equiptment_update_loop == true && pda_m_type() != PM_EQUIPTMENT_STATUS)
      {
        LOG(PDA_LOG,LOG_DEBUG, "**** PDA End Equiptment loop ****\n");
        // Moved onto different MENU.  Probably need to update any pump changes
        equiptment_update_loop = false;

        // End of equiptment status chain of menus, reset any pump or equiptment that wasn't listed in menus as long as we are not in programming mode
        if (!in_programming_mode(_aqualink_data) ) {
          pda_pump_update(_aqualink_data, -1);
          equiptment_update_cycle(-1);
        }
      } 
      else if (_initWithRS == false && pda_m_type() == PM_FW_VERSION) 
      {
        _initWithRS = true;
        LOG(PDA_LOG,LOG_DEBUG, "**** PDA INIT ****\n");
        //printf("**** PDA INIT PUT BACK IN ****\n");
        queueGetProgramData(AQUAPDA, _aqualink_data);
      }
    break;

    case CMD_MSG_LONG:
      msg = (char *)packet + PKT_DATA + 1;
      index = packet[PKT_DATA] & 0xF;
      if (packet[PKT_DATA] == 0x82)
      { // Air & Water temp is always this ID
        process_pda_packet_msg_long_temp(msg);
      }
      else if (packet[PKT_DATA] == 0x40)
      { // Time is always on this ID
        process_pda_packet_msg_long_time(msg);
      }
      else 
      {
        switch (pda_m_type()) 
        {
          case PM_EQUIPTMENT_CONTROL:
            process_pda_packet_msg_long_equipment_control(msg);
          break;
          case PM_HOME:
          case PM_BUILDING_HOME:
            process_pda_packet_msg_long_home(msg);
          break;
          case PM_EQUIPTMENT_STATUS:
            process_pda_packet_msg_long_equiptment_status(msg, index, false);
          break;
          case PM_SET_TEMP:
            process_pda_packet_msg_long_set_temp(msg);
          break;
          case PM_SPA_HEAT:
            process_pda_packet_msg_long_spa_heat(msg);
          break;
          case PM_POOL_HEAT:
            process_pda_packet_msg_long_pool_heat(msg);
          break;
          case PM_FREEZE_PROTECT:
            process_pda_packet_msg_long_freeze_protect(msg);
          break;
          case PM_AQUAPURE:
            process_pda_packet_msg_long_SWG(msg);
          break;
          case PM_AUX_LABEL_DEVICE:
            process_pda_packet_msg_long_level_aux_device(msg);
          break;
          case PM_SET_TIME:
            process_pda_packet_msg_long_set_time(msg);
          break;
          //case PM_FW_VERSION:
          //  process_pda_packet_msg_long_FW_version(msg);
          //break;
          case PM_UNKNOWN:
          default:
            process_pda_packet_msg_long_unknown(msg);
          break;
        }
      }
    break;

    case CMD_PDA_0x1B:
      LOG(PDA_LOG,LOG_DEBUG, "**** CMD_PDA_0x1B ****\n");
    // We get two of these on startup, one with 0x00 another with 0x01 at index 4.  Just act on one.
    // Think this is PDA finishd showing startup screen
      if (packet[4] == 0x00) { 
        if (_initWithRS == false)
        {
          _initWithRS = true;
          LOG(PDA_LOG,LOG_DEBUG, "**** PDA INIT ****\n");
        //aq_programmer(AQ_PDA_INIT, NULL, _aqualink_data);
          queueGetProgramData(AQUAPDA, _aqualink_data);
#ifdef BETA_PDA_AUTOLABEL
          if (_aqconfig_->use_panel_aux_labels)
             aq_programmer(AQ_GET_AUX_LABELS, NULL, _aqualink_data);
#endif
        } else {
          LOG(PDA_LOG,LOG_DEBUG, "**** PDA WAKE INIT ****\n");
          aq_programmer(AQ_PDA_WAKE_INIT, NULL, _aqualink_data);
        }
      }
    break;
  }

  if (packet[PKT_CMD] == CMD_MSG_LONG || packet[PKT_CMD] == CMD_PDA_HIGHLIGHT || 
      packet[PKT_CMD] == CMD_PDA_SHIFTLINES || packet[PKT_CMD] == CMD_PDA_CLEAR ||
      packet[PKT_CMD] == CMD_PDA_HIGHLIGHTCHARS)
  {
    // We processed the next message, kick any threads waiting on the message.
    kick_aq_program_thread(_aqualink_data, AQUAPDA);
  }
  
  // HERE AS A TEST.  NEED TO FULLY TEST THIS IS GOES TO PROD.
  else if (packet[PKT_CMD] == CMD_STATUS)
  {
    kick_aq_program_thread(_aqualink_data, AQUAPDA);
  }
  

  return rtn;
}


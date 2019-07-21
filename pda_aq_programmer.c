
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>


#include "aqualink.h"
#include "utils.h"
#include "aq_programmer.h"
#include "aq_serial.h"
#include "pda.h"
#include "pda_menu.h"
#include "pda_aq_programmer.h"

#include "init_buttons.h"

#include <time.h>
#include "timespec_subtract.h"

bool waitForPDAMessageHighlight(struct aqualinkdata *aq_data, int highlighIndex, int numMessageReceived);
bool waitForPDAMessageType(struct aqualinkdata *aq_data, unsigned char mtype, int numMessageReceived);
bool waitForPDAMessageTypes(struct aqualinkdata *aq_data, unsigned char mtype1, unsigned char mtype2, int numMessageReceived);
bool waitForPDAMessageTypesOrMenu(struct aqualinkdata *aq_data, unsigned char mtype1, unsigned char mtype2, int numMessageReceived, char *text, int line);
bool goto_pda_menu(struct aqualinkdata *aq_data, pda_menu_type menu);
bool wait_pda_selected_item(struct aqualinkdata *aq_data);
bool waitForPDAnextMenu(struct aqualinkdata *aq_data);
bool loopover_devices(struct aqualinkdata *aq_data);
bool find_pda_menu_item(struct aqualinkdata *aq_data, char *menuText, int charlimit);
bool select_pda_menu_item(struct aqualinkdata *aq_data, char *menuText, bool waitForNextMenu);

static pda_type _PDA_Type;

/* 
// Each RS message / call to this function is around 0.2 seconds apart
//#define MAX_ACK_FOR_THREAD 200 // ~40 seconds (Init takes 30)
#define MAX_ACK_FOR_THREAD 60 // ~12 seconds (testing, will stop every thread)

// *** DELETE THIS WHEN PDA IS OUT OF BETA ****
void pda_programming_thread_check(struct aqualinkdata *aq_data)
{
  static pthread_t thread_id = 0;
  static int ack_count = 0;
  static struct timespec start;
  static struct timespec now;
  struct timespec elapsed;

  // Check for long lasting threads
  if (aq_data->active_thread.thread_id != 0) {
    if (thread_id != *aq_data->active_thread.thread_id) {
       printf ("**************** LAST POINTER SET %ld , %p ****************************\n",thread_id,&thread_id);
     
      thread_id = *aq_data->active_thread.thread_id;
      clock_gettime(CLOCK_REALTIME, &start);
      printf ("**************** NEW POINTER SET %d, %ld %ld , %p %p ****************************\n",aq_data->active_thread.ptype,thread_id,*aq_data->active_thread.thread_id,&thread_id,aq_data->active_thread.thread_id);
      ack_count = 0;
    } else if (ack_count > MAX_ACK_FOR_THREAD) {
       clock_gettime(CLOCK_REALTIME, &now);
       timespec_subtract(&elapsed, &now, &start);
       logMessage(LOG_ERR, "Thread %d,%p FAILED to finished in reasonable time, %d.%03ld sec, killing it.\n",
             aq_data->active_thread.ptype,
             aq_data->active_thread.thread_id,
             elapsed.tv_sec, elapsed.tv_nsec / 1000000L);

      if (pthread_cancel(*aq_data->active_thread.thread_id) != 0)
          logMessage(LOG_ERR, "Thread kill failed\n");
      else {
       
      }
      aq_data->active_thread.thread_id = 0;
      aq_data->active_thread.ptype = AQP_NULL;
      ack_count = 0;
      thread_id = 0;
    } else {
      ack_count++;
    }
  } else {
    ack_count = 0;
    thread_id = 0;
  }
}
*/

bool wait_pda_selected_item(struct aqualinkdata *aq_data)
{
  int i=0;

  while (pda_m_hlightindex() == -1 && i < 5){
    waitForPDAMessageType(aq_data,CMD_PDA_HIGHLIGHT,10);
    i++;
  }

  if (pda_m_hlightindex() == -1)
    return false;
  else
   return true;
}

bool waitForPDAnextMenu(struct aqualinkdata *aq_data) {
  waitForPDAMessageType(aq_data,CMD_PDA_CLEAR,10);
  return waitForPDAMessageTypes(aq_data,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,15);
}

bool loopover_devices(struct aqualinkdata *aq_data) {
  int i;

  if (! goto_pda_menu(aq_data, PM_EQUIPTMENT_CONTROL)) {
    //logMessage(LOG_ERR, "PDA :- can't find main menu\n");
    //cleanAndTerminateThread(threadCtrl);
    return false;
  }
  
  // Should look for message "ALL OFF", that's end of device list.
  for (i=0; i < 18 && pda_find_m_index("ALL OFF") == -1 ; i++) {
    send_cmd(KEY_PDA_DOWN);
    //while (get_aq_cmd_length() > 0) { delay(200); }
    //waitForPDAMessageType(aq_data,CMD_PDA_HIGHLIGHT,3);
    waitForMessage(aq_data, NULL, 1);
  }

  return true;
}

/*
  if charlimit is set, use case insensitive match and limit chars.
*/
bool find_pda_menu_item(struct aqualinkdata *aq_data, char *menuText, int charlimit) {
  int i=pda_m_hlightindex();

  logMessage(LOG_DEBUG, "PDA Device programmer looking for menu text '%s'\n",menuText);

  int index = (charlimit == 0)?pda_find_m_index(menuText):pda_find_m_index_case(menuText, charlimit);

  if (index < 0) { // No menu, is there a page down.  "PDA Line 9 =    ^^ MORE __"
    if (strncasecmp(pda_m_line(9),"   ^^ MORE", 10) == 0) {
      int j;
      for(j=0; j < 20; j++) {
        send_cmd(KEY_PDA_DOWN);
        //delay(500);
        //wait_for_empty_cmd_buffer();
        waitForPDAMessageType(aq_data,CMD_PDA_HIGHLIGHT,2);
        //waitForMessage(aq_data, NULL, 1);
        index = (charlimit == 0)?pda_find_m_index(menuText):pda_find_m_index_case(menuText, charlimit);
        if (index >= 0) {
          i=pda_m_hlightindex();
          break;
        }
      }
      if (index < 0) {
        logMessage(LOG_ERR, "PDA Device programmer couldn't find menu item on any page '%s'\n",menuText);
        return false;
      }
    } else {
      logMessage(LOG_ERR, "PDA Device programmer couldn't find menu item '%s'\n",menuText);
      return false;
    }
  }

  // Found the text we want in the menu, now move to that position and select it.
  //logMessage(LOG_DEBUG, "******************PDA Device programmer menu text '%s' is at index %d\n",menuText, index);

  if (i < index) {
    for (i=pda_m_hlightindex(); i < index; i++) {
      //logMessage(LOG_DEBUG, "******************PDA queue down index %d\n",i);
      send_cmd(KEY_PDA_DOWN);
    }
  } else if (i > index) {
    for (i=pda_m_hlightindex(); i > index; i--) {
      //logMessage(LOG_DEBUG, "******************PDA queue down index %d\n",i);
      send_cmd(KEY_PDA_UP);
    }
  }

  return waitForPDAMessageHighlight(aq_data, index, 10);
}

bool select_pda_menu_item(struct aqualinkdata *aq_data, char *menuText, bool waitForNextMenu) {

  if ( find_pda_menu_item(aq_data, menuText, 0) ) {
    send_cmd(KEY_PDA_SELECT);

    logMessage(LOG_DEBUG, "PDA Device programmer selected menu item '%s'\n",menuText);
    if (waitForNextMenu)
      waitForPDAnextMenu(aq_data);

    return true;
  }

  logMessage(LOG_ERR, "PDA Device programmer couldn't selected menu item '%s' at index %d\n",menuText, index);
  return false;
}

bool goto_pda_menu(struct aqualinkdata *aq_data, pda_menu_type menu) {
  int i = 0;
  //char *menuText;

  logMessage(LOG_DEBUG, "PDA Device programmer request for menu %d\n",menu);

  
  while (pda_m_type() == PM_FW_VERSION || pda_m_type() == PM_BUILDING_HOME) {
    //logMessage(LOG_DEBUG, "******************PDA Device programmer delay on firmware or building home menu\n");
    // :TODO: remove this delay
    delay(500);
  }

  // Keep going back, checking each time to get to home.
  while ( pda_m_type() != menu && pda_m_type() != PM_HOME) {
    if (pda_m_type() != PM_BUILDING_HOME) {
      send_cmd(KEY_PDA_BACK);
      //logMessage(LOG_DEBUG, "******************PDA Device programmer selected back button\n",menu);
      waitForPDAnextMenu(aq_data);
    } else {
      waitForPDAMessageType(aq_data,CMD_PDA_HIGHLIGHT,15);
    }

    if (i > 4 ) {
      logMessage(LOG_ERR, "PDA Device programmer request for menu %d failed! Couldn't get to HOME menu\n",menu);
      return false;
    }
    i++;
    //logMessage(LOG_DEBUG, "******************PDA Device programmer menu type %d\n",pda_m_type());
    //if (!wait_for_empty_cmd_buffer() || i++ > 6)
    //  return false;
  }
  

  if (pda_m_type() == menu)
    return true;

  switch (menu) {
    case PM_EQUIPTMENT_CONTROL:
      select_pda_menu_item(aq_data, "EQUIPMENT ON/OFF", true);
    break;
    case PM_PALM_OPTIONS:
      select_pda_menu_item(aq_data, "MENU", true);
      select_pda_menu_item(aq_data, "PALM OPTIONS", true);
    case PM_AUX_LABEL:
      if ( _PDA_Type == PDA) {
        select_pda_menu_item(aq_data, "MENU", true);
        select_pda_menu_item(aq_data, "SYSTEM SETUP", true); // This is a guess, (I have rev#)
        select_pda_menu_item(aq_data, "LABEL AUX", true);
      } else {
        logMessage(LOG_ERR, "PDA in AquaPlalm mode, there is no SYSTEM SETUP / LABEL AUX menu\n");
      }
    break;
    case PM_SYSTEM_SETUP:
      if ( _PDA_Type == PDA) {
        select_pda_menu_item(aq_data, "MENU", true);
        select_pda_menu_item(aq_data, "SYSTEM SETUP", true); // This is a guess, (I have rev#)
      } else {
        logMessage(LOG_ERR, "PDA in AquaPlalm mode, there is no SYSTEM SETUP menu\n");
      }
    break;
    case PM_FREEZE_PROTECT:
      if ( _PDA_Type == PDA) {
        select_pda_menu_item(aq_data, "MENU", true);
        select_pda_menu_item(aq_data, "SYSTEM SETUP", true); // This is a guess, (I have rev#)
        select_pda_menu_item(aq_data, "FREEZE PROTECT", true); // This is a guess, (I have rev#)
      } else {
        logMessage(LOG_ERR, "PDA in AquaPlalm mode, there is no SYSTEM SETUP / FREEZE PROTECT menu\n");
      }
    break;
    case PM_AQUAPURE:
      select_pda_menu_item(aq_data, "MENU", true);
      select_pda_menu_item(aq_data, "SET AquaPure", true);
    break;
    case PM_SET_TEMP:
      select_pda_menu_item(aq_data, "MENU", true);
      // Depending on control panel config, may get an extra menu asking to press any key.
      select_pda_menu_item(aq_data, "SET TEMP", false);
      waitForPDAMessageType(aq_data,CMD_PDA_CLEAR,10);
      waitForPDAMessageTypesOrMenu(aq_data,CMD_PDA_HIGHLIGHT,CMD_PDA_HIGHLIGHTCHARS,20,"press ANY key",8);
    break;
    case PM_SET_TIME:
      select_pda_menu_item(aq_data, "MENU", true);
      select_pda_menu_item(aq_data, "SET TIME", true);
    break;
    default:
      logMessage(LOG_ERR, "PDA Device programmer didn't understand requested menu\n");
      return false;
    break;
  }

  if (pda_m_type() != menu) {
    logMessage(LOG_ERR, "PDA Device programmer didn't find a requested menu\n");
    //return true;
    return false;
  }

  logMessage(LOG_DEBUG, "PDA Device programmer request for menu %d found\n",menu);
  
  return true;
}



void *set_aqualink_PDA_device_on_off( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  //int i=0;
  //int found;
  char device_name[15];
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_DEVICE_ON_OFF);
  
  char *buf = (char*)threadCtrl->thread_args;
  unsigned int device = atoi(&buf[0]);
  unsigned int state = atoi(&buf[5]);

  if (device > TOTAL_BUTTONS) {
    logMessage(LOG_ERR, "PDA Device On/Off :- bad device number '%d'\n",device);
    cleanAndTerminateThread(threadCtrl);
    return NULL;
  }

  logMessage(LOG_INFO, "PDA Device On/Off, device '%s', state %d\n",aq_data->aqbuttons[device].pda_label,state);

  if (! goto_pda_menu(aq_data, PM_EQUIPTMENT_CONTROL)) {
    logMessage(LOG_ERR, "PDA Device On/Off :- can't find main menu\n");
    cleanAndTerminateThread(threadCtrl);
    return NULL;
  }

  // If single config (Spa OR pool) rather than (Spa AND pool) heater is TEMP1 and TEMP2
  if (aq_data->single_device == TRUE && device == POOL_HEAT_INDEX) { // rename Heater and Spa
    snprintf(device_name, sizeof(device_name), "%-13s\n","TEMP1");
  } else if (aq_data->single_device == TRUE && device == SPA_HEAT_INDEX)  {// rename Heater and Spa
    snprintf(device_name, sizeof(device_name), "%-13s\n","TEMP2");
  } else {
    //Pad name with spaces so something like "SPA" doesn't match "SPA BLOWER"
    snprintf(device_name, sizeof(device_name), "%-13s\n",aq_data->aqbuttons[device].pda_label);
  }

  if ( find_pda_menu_item(aq_data, device_name, 13) ) {
    if (aq_data->aqbuttons[device].led->state != state) {
      logMessage(LOG_INFO, "PDA Device On/Off, found device '%s', changing state\n",aq_data->aqbuttons[device].pda_label,state);
      send_cmd(KEY_PDA_SELECT);
      //while (get_aq_cmd_length() > 0) { delay(500); }
    } else {
      logMessage(LOG_INFO, "PDA Device On/Off, found device '%s', not changing state, is same\n",aq_data->aqbuttons[device].pda_label,state);
    }
  } else {
    logMessage(LOG_ERR, "PDA Device On/Off, device '%s' not found\n",aq_data->aqbuttons[device].pda_label);
  }

  //goto_pda_menu(aq_data, PM_HOME);

  cleanAndTerminateThread(threadCtrl);
  
  return NULL;

}



void *get_aqualink_PDA_device_status( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_DEVICE_STATUS);
  
  if (! loopover_devices(aq_data)) {
    logMessage(LOG_ERR, "PDA Device Status :- failed\n");
  }
 
  goto_pda_menu(aq_data, PM_HOME);

  cleanAndTerminateThread(threadCtrl);
  
  return NULL;
}

void *set_aqualink_PDA_init( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;
  //int i=0;

  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_INIT);
  
  //int val = atoi((char*)threadCtrl->thread_args);

  //logMessage(LOG_DEBUG, "PDA Init\n", val);

  logMessage(LOG_DEBUG, "PDA Init\n");

  if (pda_m_type() == PM_FW_VERSION) {
    // check pda_m_line(1) to "AquaPalm"
    if (strstr(pda_m_line(1), "AquaPalm") != NULL) {
      _PDA_Type = AQUAPALM;
    } else {
      _PDA_Type = PDA;
    }
    char *ptr1 = pda_m_line(1);
    char *ptr2 = pda_m_line(5);
    ptr1[AQ_MSGLEN+1] = '\0';
    ptr2[AQ_MSGLEN+1] = '\0';
    //strcpy(aq_data->version, stripwhitespace(ptr));
    snprintf(aq_data->version, (AQ_MSGLEN*2)-1, "%s %s",stripwhitespace(ptr1),stripwhitespace(ptr2));

    //printf("****** Version '%s' ********\n",aq_data->version);
  }
/* 
  // Get status of all devices
  if (! loopover_devices(aq_data)) {
    logMessage(LOG_ERR, "PDA Init :- can't find menu\n");
  }
*/
  // Get heater setpoints
  if (! get_PDA_aqualink_pool_spa_heater_temps(aq_data)) {
    logMessage(LOG_ERR, "PDA Init :- Error getting heater setpoints\n");
  }

  // Get freeze protect setpoint, AquaPalm doesn't have freeze protect in menu.
  if (_PDA_Type != AQUAPALM && ! get_PDA_freeze_protect_temp(aq_data)) {
    logMessage(LOG_ERR, "PDA Init :- Error getting freeze setpoints\n");
  }

  goto_pda_menu(aq_data, PM_HOME);

  pda_reset_sleep();

  cleanAndTerminateThread(threadCtrl);

  return NULL;
}


void *set_aqualink_PDA_wakeinit( void *ptr )
{
  struct programmingThreadCtrl *threadCtrl;
  threadCtrl = (struct programmingThreadCtrl *) ptr;
  struct aqualinkdata *aq_data = threadCtrl->aq_data;

  // At this point, we should probably just exit if there is a thread already going as 
  // it means the wake was called due to changing a device.
  waitForSingleThreadOrTerminate(threadCtrl, AQ_PDA_WAKE_INIT);

  logMessage(LOG_DEBUG, "PDA Wake Init\n");

  // Get status of all devices
  if (! loopover_devices(aq_data)) {
    logMessage(LOG_ERR, "PDA Init :- can't find menu\n");
  }

  cleanAndTerminateThread(threadCtrl);
  
  return NULL;
}


bool get_PDA_freeze_protect_temp(struct aqualinkdata *aq_data) {
  
  if ( _PDA_Type == PDA) {
    if (! goto_pda_menu(aq_data, PM_FREEZE_PROTECT)) {   
      return false;
    }
  } else {
    logMessage(LOG_INFO, "In PDA AquaPalm mode, freezepoints not supported\n");
    return false;
  }
  
  return true;
}

bool get_PDA_aqualink_pool_spa_heater_temps(struct aqualinkdata *aq_data) {
  
   // Get heater setpoints
  if (! goto_pda_menu(aq_data, PM_SET_TEMP)) {
    return false;
  }
  
  return true;
}

bool waitForPDAMessageHighlight(struct aqualinkdata *aq_data, int highlighIndex, int numMessageReceived)
{
  logMessage(LOG_DEBUG, "waitForPDAMessageHighlight index %d\n",highlighIndex);

  if(pda_m_hlightindex() == highlighIndex) return true;

  int i=0;
  pthread_mutex_lock(&aq_data->active_thread.thread_mutex);

  while( ++i <= numMessageReceived)
  {
    logMessage(LOG_DEBUG, "waitForPDAMessageHighlight last = 0x%02hhx : index %d : (%d of %d)\n",aq_data->last_packet_type,pda_m_hlightindex(),i,numMessageReceived);

    if (aq_data->last_packet_type == CMD_PDA_HIGHLIGHT && pda_m_hlightindex() == highlighIndex) break;

    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
  }

  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);
  
  if (pda_m_hlightindex() != highlighIndex) {
    //logMessage(LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    logMessage(LOG_DEBUG, "waitForPDAMessageHighlight: did not receive index '%d'\n",highlighIndex);
    return false;
  } else 
    logMessage(LOG_DEBUG, "waitForPDAMessageHighlight: received index '%d'\n",highlighIndex);
  
  return true;
}


bool waitForPDAMessageType(struct aqualinkdata *aq_data, unsigned char mtype, int numMessageReceived)
{
  logMessage(LOG_DEBUG, "waitForPDAMessageType  0x%02hhx\n",mtype);

  int i=0;
  pthread_mutex_lock(&aq_data->active_thread.thread_mutex);

  while( ++i <= numMessageReceived)
  {
    logMessage(LOG_DEBUG, "waitForPDAMessageType 0x%02hhx, last message type was 0x%02hhx (%d of %d)\n",mtype,aq_data->last_packet_type,i,numMessageReceived);

    if (aq_data->last_packet_type == mtype) break;

    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
  }

  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);
  
  if (aq_data->last_packet_type != mtype) {
    //logMessage(LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    logMessage(LOG_DEBUG, "waitForPDAMessageType: did not receive 0x%02hhx\n",mtype);
    return false;
  } else 
    logMessage(LOG_DEBUG, "waitForPDAMessageType: received 0x%02hhx\n",mtype);
  
  return true;
}

// Wait for Message, hit return on particular menu.
bool waitForPDAMessageTypesOrMenu(struct aqualinkdata *aq_data, unsigned char mtype1, unsigned char mtype2, int numMessageReceived, char *text, int line)
{
  logMessage(LOG_DEBUG, "waitForPDAMessageTypes  0x%02hhx or 0x%02hhx\n",mtype1,mtype2);

  int i=0;
  bool gotmenu = false;
  pthread_mutex_init(&aq_data->active_thread.thread_mutex, NULL);
  pthread_mutex_lock(&aq_data->active_thread.thread_mutex);

  while( ++i <= numMessageReceived)
  {
    if (gotmenu == false && line > 0 && text != NULL) {
      if (stristr(pda_m_line(line), text) != NULL) {
        send_cmd(KEY_PDA_SELECT);
        gotmenu = true;
        logMessage(LOG_DEBUG, "waitForPDAMessageTypesOrMenu saw '%s' and line %d\n",text,line);
      }
    }
    logMessage(LOG_DEBUG, "waitForPDAMessageTypes 0x%02hhx | 0x%02hhx, last message type was 0x%02hhx (%d of %d)\n",mtype1,mtype2,aq_data->last_packet_type,i,numMessageReceived);

    if (aq_data->last_packet_type == mtype1 || aq_data->last_packet_type == mtype2) break;

    pthread_cond_init(&aq_data->active_thread.thread_cond, NULL);
    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
  }

  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);
  
  if (aq_data->last_packet_type != mtype1 && aq_data->last_packet_type != mtype2) {
    //logMessage(LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    logMessage(LOG_ERR, "waitForPDAMessageTypes: did not receive 0x%02hhx or 0x%02hhx\n",mtype1,mtype2);
    return false;
  } else 
    logMessage(LOG_DEBUG, "waitForPDAMessageTypes: received 0x%02hhx\n",aq_data->last_packet_type);
  
  return true;
}

bool waitForPDAMessageTypes(struct aqualinkdata *aq_data, unsigned char mtype1, unsigned char mtype2, int numMessageReceived)
{
  return waitForPDAMessageTypesOrMenu(aq_data, mtype1, mtype2, numMessageReceived, NULL, 0);
}

bool set_PDA_numeric_field_value(struct aqualinkdata *aq_data, int val, int *cur_val, char *select_label, int step) {
  int i=0;

  // Should probably change below to call find_pda_menu_item(), rather than doing it here
  // If we lease this, need to limit on the number of loops
  while ( strncasecmp(pda_m_hlight(), select_label, 8) != 0 ) {
    send_cmd(KEY_PDA_DOWN);
    delay(500);  // Last message probably was CMD_PDA_HIGHLIGHT, so wait before checking.
    waitForPDAMessageType(aq_data,CMD_PDA_HIGHLIGHT,2);
    if (i > 10) {
      logMessage(LOG_ERR, "PDA numeric selector could not find string '%s'\n", select_label);
      return false;
    }
    i++;
  }

  send_cmd(KEY_PDA_SELECT);

  if (val < *cur_val) {
    logMessage(LOG_DEBUG, "PDA %s value : lower from %d to %d\n", select_label, *cur_val, val);
    for (i = *cur_val; i > val; i=i-step) {
      send_cmd(KEY_PDA_DOWN);
    }
  } else if (val > *cur_val) {
    logMessage(LOG_DEBUG, "PDA %s value : raise from %d to %d\n", select_label, *cur_val, val);
    for (i = *cur_val; i < val; i=i+step) {
      send_cmd(KEY_PDA_UP);
    }
  } else {
    logMessage(LOG_INFO, "PDA %s value : already at %d\n", select_label, val);
    send_cmd(KEY_PDA_BACK);
    return true;
  }

  send_cmd(KEY_PDA_SELECT);
  logMessage(LOG_DEBUG, "PDA %s value : set to %d\n", select_label, *cur_val);
  
  return true;
}

bool set_PDA_aqualink_SWG_setpoint(struct aqualinkdata *aq_data, int val) {
  
  if (! goto_pda_menu(aq_data, PM_AQUAPURE)) {
    logMessage(LOG_ERR, "Error finding SWG setpoints menu\n");
  }

  if (aq_data->aqbuttons[SPA_INDEX].led->state != OFF) 
    return set_PDA_numeric_field_value(aq_data, val, &aq_data->swg_percent, "SET SPA", 5);
  else
    return set_PDA_numeric_field_value(aq_data, val, &aq_data->swg_percent, "SET POOL", 5);
  
  //return true;
}

bool set_PDA_aqualink_heater_setpoint(struct aqualinkdata *aq_data, int val, bool isPool) {
  char label[10];
  int *cur_val;

  if ( aq_data->single_device != true ) {
    if (isPool) {
      sprintf(label, "POOL HEAT");
      cur_val = &aq_data->pool_htr_set_point;
    } else {
      sprintf(label, "SPA HEAT");
      cur_val = &aq_data->spa_htr_set_point;
    }
  } else {
    if (isPool) {
      sprintf(label, "TEMP1");
      cur_val = &aq_data->pool_htr_set_point;
    } else {
      sprintf(label, "TEMP2");
      cur_val = &aq_data->spa_htr_set_point;
    }
  }

  if (val == *cur_val) {
    logMessage(LOG_INFO, "PDA %s setpoint : temp already %d\n", label, val);
    send_cmd(KEY_PDA_BACK);
    return true;
  } 

  if (! goto_pda_menu(aq_data, PM_SET_TEMP)) {
    logMessage(LOG_ERR, "Error finding heater setpoints menu\n");
    return false;
  }

  set_PDA_numeric_field_value(aq_data, val, cur_val, label, 1);

  return true;
}

bool set_PDA_aqualink_freezeprotect_setpoint(struct aqualinkdata *aq_data, int val) {
  
  if (! goto_pda_menu(aq_data, PM_FREEZE_PROTECT)) {
    logMessage(LOG_ERR, "Error finding freeze protect setpoints menu\n");
    return false;
  }

  return set_PDA_numeric_field_value(aq_data, val, &aq_data->frz_protect_set_point, "TEMP", 1);

  //return true;
}

// Test ine this.
bool get_PDA_aqualink_aux_labels(struct aqualinkdata *aq_data) {
#ifdef BETA_PDA_AUTOLABEL
  int i=0;
  char label[10];

  logMessage(LOG_INFO, "Finding PDA labels, (BETA ONLY)\n");

  if (! goto_pda_menu(aq_data, PM_AUX_LABEL)) {
    logMessage(LOG_ERR, "Error finding aux label menu\n");
    return false;
  }

  for (i=1;i<8;i++) {
    sprintf(label, "AUX%d",i);
    select_pda_menu_item(aq_data, label, true);
    send_cmd(KEY_PDA_BACK);
    waitForPDAnextMenu(aq_data);
  }

  // Read first page of devices and make some assumptions.
#endif
  return true;
}

/*
bool waitForPDAMessage(struct aqualinkdata *aq_data, int numMessageReceived, unsigned char packettype)
{
  logMessage(LOG_DEBUG, "waitForPDAMessage %s %d\n",message,numMessageReceived);
  int i=0;
  pthread_mutex_init(&aq_data->active_thread.thread_mutex, NULL);
  pthread_mutex_lock(&aq_data->active_thread.thread_mutex);
  char* msgS;
  char* ptr;
  
  if (message != NULL) {
    if (message[0] == '^')
      msgS = &message[1];
    else
      msgS = message;
  }
  
  while( ++i <= numMessageReceived)
  {
    if (message != NULL)
      logMessage(LOG_DEBUG, "Programming mode: loop %d of %d looking for '%s' received message '%s'\n",i,numMessageReceived,message,aq_data->last_message);
    else
      logMessage(LOG_DEBUG, "Programming mode: loop %d of %d waiting for next message, received '%s'\n",i,numMessageReceived,aq_data->last_message);

    if (message != NULL) {
      ptr = stristr(aq_data->last_message, msgS);
      if (ptr != NULL) { // match
        logMessage(LOG_DEBUG, "Programming mode: String MATCH\n");
        if (msgS == message) // match & don't care if first char
          break;
        else if (ptr == aq_data->last_message) // match & do care if first char
          break;
      }
    }
    
    //logMessage(LOG_DEBUG, "Programming mode: looking for '%s' received message '%s'\n",message,aq_data->last_message);
    pthread_cond_init(&aq_data->active_thread.thread_cond, NULL);
    pthread_cond_wait(&aq_data->active_thread.thread_cond, &aq_data->active_thread.thread_mutex);
    //logMessage(LOG_DEBUG, "Programming mode: loop %d of %d looking for '%s' received message '%s'\n",i,numMessageReceived,message,aq_data->last_message);
  }
  
  pthread_mutex_unlock(&aq_data->active_thread.thread_mutex);
  
  if (message != NULL && ptr == NULL) {
    //logMessage(LOG_ERR, "Could not select MENU of Aqualink control panel\n");
    logMessage(LOG_DEBUG, "Programming mode: did not find '%s'\n",message);
    return false;
  } else if (message != NULL)
    logMessage(LOG_DEBUG, "Programming mode: found message '%s' in '%s'\n",message,aq_data->last_message);
  
  return true;
}

*/


/*
Link to two different menu's used in PDA
http://www.poolequipmentpriceslashers.com.au/wp-content/uploads/2012/11/Jandy-Aqualink-RS-PDA-Wireless-Pool-Controller_manual.pdf
https://www.jandy.com/-/media/zodiac/global/downloads/h/h0574200.pdf
*/

/*
  List of how menu's display

PDA Line 0 =
PDA Line 1 =     AquaPalm
PDA Line 2 =
PDA Line 3 = Firmware Version
PDA Line 4 =
PDA Line 5 =     REV MMM
PDA Line 6 =
PDA Line 7 =
PDA Line 8 =
PDA Line 9 =

PDA Line 0 = 
PDA Line 1 =     AquaPalm    
PDA Line 2 = 
PDA Line 3 = Firmware Version
PDA Line 4 = 
PDA Line 5 =      REV T      
PDA Line 6 = 
PDA Line 7 = 
PDA Line 8 = 
PDA Line 9 = 

PDA Menu Line 0 = 
PDA Menu Line 1 =   PDA-P4 Only   
PDA Menu Line 2 = 
PDA Menu Line 3 = Firmware Version
PDA Menu Line 4 = 
PDA Menu Line 5 =      PDA: 7.1.0 
PDA Menu Line 6 = 
PDA Menu Line 7 = 
PDA Menu Line 8 = 
PDA Menu Line 9 = 

************** The above have different menu to below rev/version *********
***************** Think this is startup different rev *************
PDA Menu Line 0 =
PDA Menu Line 1 =  PDA-PS4 Combo
PDA Menu Line 2 =
PDA Menu Line 3 = Firmware Version
PDA Menu Line 4 =
PDA Menu Line 5 =   PPD: PDA 1.2

PDA Line 0 =
PDA Line 1 = AIR         POOL
PDA Line 2 =
PDA Line 3 =
PDA Line 4 = POOL MODE     ON
PDA Line 5 = POOL HEATER  OFF
PDA Line 6 = SPA MODE     OFF
PDA Line 7 = SPA HEATER   OFF
PDA Line 8 = MENU
PDA Line 9 = EQUIPMENT ON/OFF

PDA Line 0 =    MAIN MENU
PDA Line 1 =
PDA Line 2 = SET TEMP       >
PDA Line 3 = SET TIME       >
PDA Line 4 = SET AquaPure   >
PDA Line 5 = PALM OPTIONS   >
PDA Line 6 =
PDA Line 7 =    BOOST POOL
PDA Line 8 =
PDA Line 9 =

**************** OPTION 2 FOR THIS MENU ********************
PDA Line 0 = MAIN MENU
PDA Line 1 =
PDA Line 2 = HELP >
PDA Line 3 = PROGRAM >
PDA Line 4 = SET TEMP >
PDA Line 5 = SET TIME >
PDA Line 6 = PDA OPTIONS >
PDA Line 7 = SYSTEM SETUP >
PDA Line 8 =
PDA Line 9 = BOOST

********** Guess at SYSTEM SETUP Menu  (not on Rev MMM or before)************

// PDA Line 0 =   SYSTEM SETUP
// PDA Line 1 = LABEL AUX      >
// PDA Line 2 = FREEZE PROTECT >
// PDA Line 3 = AIR TEMP       >
// PDA Line 4 = DEGREES C/F    >
// PDA Line 5 = TEMP CALIBRATE >
// PDA Line 6 = SOLAR PRIORITY >
// PDA Line 7 = PUMP LOCKOUT   >
// PDA Line 8 = ASSIGN JVAs    >
// PDA Line 9 =    ^^ MORE __
// PDA Line 5 = COLOR LIGHTS   >
// PDA Line 6 = SPA SWITCH     >
// PDA Line 7 = SERVICE INFO   >
// PDA Line 8 = CLEAR MEMORY   >



PDA Line 0 =   PALM OPTIONS
PDA Line 1 =
PDA Line 2 =
PDA Line 3 = SET AUTO-OFF   >
PDA Line 4 = BACKLIGHT      >
PDA Line 5 = ASSIGN HOTKEYS >
PDA Line 6 =
PDA Line 7 = Choose setting
PDA Line 8 = and press SELECT
PDA Line 9 =

PDA Line 0 =   SET AquaPure
PDA Line 1 =
PDA Line 2 =
PDA Line 3 = SET POOL TO: 45%
PDA Line 4 =  SET SPA TO:  0%
PDA Line 5 =
PDA Line 6 =
PDA Line 7 = Highlight an
PDA Line 8 = item and press
PDA Line 9 = SELECT

PDA Line 0 =     SET TIME
PDA Line 1 =
PDA Line 2 =   05/22/19 WED
PDA Line 3 =     10:53 AM
PDA Line 4 =
PDA Line 5 =
PDA Line 6 = Use ARROW KEYS
PDA Line 7 = to set value.
PDA Line 8 = Press SELECT
PDA Line 9 = to continue.

PDA Line 0 =     SET TEMP
PDA Line 1 =
PDA Line 2 = POOL HEAT   70`F
PDA Line 3 = SPA HEAT    98`F
PDA Line 4 =
PDA Line 5 =
PDA Line 6 =
PDA Line 7 = Highlight an
PDA Line 8 = item and press
PDA Line 9 = SELECT


******* GUSSING AT BELOW *******
when single mode (pool OR spa) not (pool AND spa) temps are different.

PDA Line 0 =     SET TEMP
PDA Line 1 =
PDA Line 2 = TEMP1       70`F
PDA Line 3 = TEMP2       98`F
PDA Line 4 =
PDA Line 5 =
PDA Line 6 =
PDA Line 7 = Highlight an
PDA Line 8 = item and press
PDA Line 9 = SELECT





PDA Line 0 =    EQUIPMENT
PDA Line 1 = FILTER PUMP   ON
PDA Line 2 = SPA          OFF
PDA Line 3 = POOL HEAT    OFF
PDA Line 4 = SPA HEAT     OFF
PDA Line 5 = CLEANER       ON
PDA Line 6 = WATERFALL    OFF
PDA Line 7 = AIR BLOWER   OFF
PDA Line 8 = LIGHT        OFF
PDA Line 9 =    ^^ MORE __

PDA Line 0 =    EQUIPMENT
PDA Line 1 = WATERFALL    OFF
PDA Line 2 = AIR BLOWER   OFF
PDA Line 3 = LIGHT        OFF
PDA Line 4 = AUX5         OFF
PDA Line 5 = EXTRA AUX    OFF
PDA Line 6 = SPA MODE     OFF
PDA Line 7 = CLEAN MODE   OFF
PDA Line 8 = ALL OFF
PDA Line 9 =

// This is from a single device setup (pool OR spa not pool AND spa)
PDA Menu Line 0 =    EQUIPMENT    
PDA Menu Line 1 = 
PDA Menu Line 2 = FILTER PUMP   ON
PDA Menu Line 3 = TEMP1        OFF
PDA Menu Line 4 = TEMP2        OFF
PDA Menu Line 5 = AUX1         OFF
PDA Menu Line 6 = Pool Light    ON
PDA Menu Line 7 = AUX3         OFF
PDA Menu Line 8 = EXTRA AUX    OFF
PDA Menu Line 9 = ALL OFF       

PDA Line 0 = Equipment Status
PDA Line 1 = 
PDA Line 2 = Intelliflo VS 1 
PDA Line 3 =      RPM: 1700  
PDA Line 4 =     Watts: 367  
PDA Line 5 = 
PDA Line 6 = 
PDA Line 7 = 
PDA Line 8 = 
PDA Line 9 = 

PDA Line 0 = Equipment Status
PDA Line 1 = 
PDA Line 2 =   AquaPure 20%  
PDA Line 3 =  Salt 4000 PPM  
PDA Line 4 = 
PDA Line 5 = 
PDA Line 6 = 
PDA Line 7 = 
PDA Line 8 = 
PDA Line 9 = 

VSP Motes.

four types of variable speed pumps, 
Jandy ePumpTM DC, 
Jandy ePumpTM AC,
IntelliFlo® 1 VF,
IntelliFlo® VS.

The SCALE setting is fixed to RPM for the Jandy ePumpTM DC, Jandy ePumpTM AC, and IntelliFlo® VS. 
The SCALE setting is fixed to GPM for the IntelliFlo® VF

There are eight (8) default speed presets for each variable speed pump. 
*/

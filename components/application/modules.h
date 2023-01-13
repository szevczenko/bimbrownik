/**
 *******************************************************************************
 * @file    modules.h
 * @author  Dmytro Shevchenko
 * @brief   Modules communicates with app_manager
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _MODULES_APP_H
#define _MODULES_APP_H

#include <stdbool.h>

#include "app_events.h"
#include "app_msg_id.h"
#include "config.h"
#include "network_manager.h"

/* Public macro --------------------------------------------------------------*/
// clang-format off
#define MODULES_START_DEFINITION( _task_id, _post_msg ) { .task_id = _task_id, .post_msg = _post_msg
#define MODULE_INIT_MSG(_msg) .init_msg_id = (_msg)
#define MODULE_END_DEFINITION },

// clang-format on

/* Public types --------------------------------------------------------------*/

typedef struct
{
  app_events_task_t task_id;
  void ( *post_msg )( app_event_t* event );
  app_msg_id_t init_msg_id;

  bool init_result;
} modules_apps_t;

/* Modules -------------------------------------------------------------------*/

static modules_apps_t modules[] = {
  MODULES_START_DEFINITION( APP_EVENT_NETWORK_MANAGER, NetworkManagerPostMsg ),
  MODULE_INIT_MSG( MSG_ID_INIT_REQ ),
  MODULE_END_DEFINITION };

#endif
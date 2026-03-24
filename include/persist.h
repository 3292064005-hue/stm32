#ifndef PERSIST_H
#define PERSIST_H

#include <stdbool.h>
#include <stdint.h>
#include "model.h"

void persist_init(void);
bool persist_is_initialized(void);
void persist_save_settings(const SettingsState *settings);
void persist_load_settings(SettingsState *settings);
void persist_save_alarm(const AlarmState *alarm);
void persist_load_alarm(AlarmState *alarm);

#endif

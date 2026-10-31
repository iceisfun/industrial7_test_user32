#include "ctl.h"
/* Single shared definition of the notification log used by every file in this area. */
c_logent c_log[LOG_MAX];
int c_nlogent;
int c_logging = 1;
int c_mi_base, c_mi_step;

#ifndef __Dust_Sensor_H
#define __Dust_Sensor_H

#include "DEV_Config.h"
#include <stdlib.h>

#define HIGH 1
#define LOW 0

#define        COV_RATIO                       0.2            //ug/mmm / mv
#define        NO_DUST_VOLTAGE                 400            //mv
#define        SYS_VOLTAGE                     3300   


void Dust_Sensor();

#endif

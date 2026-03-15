#include "Dust_Sensor.h"

float density, voltage;
int adcvalue;

/*
private function
*/
int Filter(int m)
{
    static int flag_first = 0, _buff[10], sum;
    const int _buff_max = 10;
    int i;

    if (flag_first == 0)
    {
        flag_first = 1;

        for (i = 0, sum = 0; i < _buff_max; i++)
        {
            _buff[i] = m;
            sum += _buff[i];
        }
        return m;
    }
    else
    {
        sum -= _buff[0];
        for (i = 0; i < (_buff_max - 1); i++)
        {
            _buff[i] = _buff[i + 1];
        }
        _buff[9] = m;
        sum += _buff[9];

        i = sum / 10.0;
        return i;
    }
}

void Dust_Sensor()
{
    DEV_Module_Init();
    while (1)
    {
        DEV_Digital_Write(DOUT_PIN, HIGH);
        DEV_Delay_us(280);
        adcvalue = adc_read();
        //printf("%d\n",adcvalue);
        DEV_Digital_Write(DOUT_PIN, LOW);
        adcvalue = Filter(adcvalue);
        //printf("%d\n",adcvalue);
        voltage = (SYS_VOLTAGE / 4096.0) * adcvalue * 11;
        //printf("%f\n",voltage);
        if (voltage >= NO_DUST_VOLTAGE)
        {
            voltage -= NO_DUST_VOLTAGE;

            density = voltage * COV_RATIO;
        }
        else
            density = 0;
        printf("The current dust concentration is:%4.2fug/m3\n ",density);
        DEV_Delay_ms(1000);
    }
}

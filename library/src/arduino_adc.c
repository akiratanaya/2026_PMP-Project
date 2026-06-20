#include "arduino_adc.h"

#include <avr/io.h>

void adc_init(void)
{
    ADMUX = _BV(REFS0); // Dideklarasi untuk set tegangan referensi 
    // fungsi _BV adalah singkatan dari bit value
    // Kalau dari yang ada di librarynya itu dia bilang operasi dari_BV ini
    // menulis angka 1 lalu digeser ke kiri sebanyak variabel yang ada didalam
    // nya. Jadi pada kasus ini karena adanya inisialisasi tersebut,
    // Secara biner, ADMUX diisi dengan angka 0100 0000, dimana ini artinya 
    // dia cuman konek ke voltase referensi sebesar 5V

    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
    // Pada state variable yang ini, intinya dia ngidupin si ADCSRA 
    // Pada pin yang sesuai dengan di state pada isi pada fungsi _BV nya
}


uint16_t analog_read(uint8_t channel)
{
    if (channel > 5)
    {
        return 0;
    }// ini channel nya gak bisa lebih dari 5 karena pin analog dari Arduino cuman ada 5

    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    // Intinya dia ngebuat ADMUX ini jadi bukan hanya include pin voltase referensi
    // Tapi dia juga ngeinclude channel yang saat ini sedang ditinjau
    
    ADCSRA |= _BV(ADSC);

    while ((ADCSRA & _BV(ADSC)) != 0)
    {
    }

    return ADC;
}
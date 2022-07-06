/********************************************
 *
 *  Name: Matthias Weaser
 *  Email: mweaser@usc.edu
 *  Section: 31291
 *  Assignment: Project - Temperature Monitor
 *
 ********************************************/

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include "lcd.h"
#include "ds18b20.h"
#include "project.h"
#include "encoder.h"
#include <avr/eeprom.h>
#define NUM_TONES 25

void variable_delay_us(int);
void play_note(unsigned short);
int getTemperature(void);
int tempConversion(int);
void splashScreen(void);
void init_timer0();


// State variables
enum states { TEMP, NOTE };
char button_state = TEMP;
volatile unsigned char new_state, old_state;
volatile unsigned char changed = 0;  // Flag for state change
volatile unsigned char a, b;


// Temperature variables
volatile int temp_count = 80;
char tempMax = 100;
char tempMin = 60;
int threshold = 0;
unsigned char t[2];
volatile int temperature = 0;

int final;
int f;
int f_dec;

int final_Main;
int f_Main;
int f_dec_Main;
int hotFlag = 0;

// Sound variables
volatile int count = 0;

unsigned int frequency[NUM_TONES] = /* Frequencies for natural notes from middle C (C4)
                                       up one octave to C5. */
    { 131, 139, 147, 156, 165, 176, 185, 196, 208, 220, 233, 247,
      262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494, 523 };

char *notes[25] =
{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
 "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C"};

 void init_timer1(unsigned short m)
 {
   TIMSK1 |= (1 << OCIE1A);
   TCCR1B |= (1 << WGM12);
   OCR1A = m;
   TCCR1B |= (1 << CS10);
   TCCR1B |= (1 << CS12);
 }

int main(void) {

  temp_count = eeprom_read_byte((void*) 300);
  count = eeprom_read_byte((void*) 301);

  if (temp_count > tempMax){
    temp_count = tempMax;
  }
  else if (temp_count < tempMin){
    temp_count = tempMin;
  }

  if (count > 24){
    count = 24;
  }
  else if (count < 0){
    count = 0;
  }

    // Initialize DDR and PORT registers and LCD
    DDRB |= (1 << PB4); // Buzzer
    PORTC |= (1 << PC1); // Encoder
    PORTC |= (1 << PC5); // Encoder
    PORTC |= (1 << PC2); // Temperature threshold set button
    PORTC |= (1 << PC3); // 1-Wire bus to ds18
    PORTC |= (1 << PC4); // Alert note set button
    DDRD |= (1 << PD2);
    DDRD |= (1 << PD3);
    PORTD &= ~(1 << PD2); // Red LED
    PORTD &= ~(1 << PD3); // Green LED

    TCCR1B &= ~(1 << CS10);
    TCCR1B &= ~(1 << CS11);
    TCCR1B &= ~(1 << CS12);

    lcd_init();
    ds_init();

    PCICR |= (1 << PCIE1);
    PCMSK1 |= (1 << PCINT9);
    PCMSK1 |= (1 << PCINT13);
    sei();

    splashScreen(); // Function call to show splash screen and initial numbers

    PORTD |= (1 << PD3);

    b = PINC & (1 << PC1); // Reading the A and B inputs to determine the intial state.
    a = PINC & (1 << PC5);

    if (!b && !a){
       old_state = 0;
    }
    else if (!b && a){
      old_state = 1;
    }
    else if (b && !a){
      old_state = 2;
    }
    else{
      old_state = 3;
    }
    new_state = old_state;

    init_timer0();

    while (1) { // Loop forever
      temp_count = eeprom_read_byte((void*) 300);
      count = eeprom_read_byte((void*) 301);

      if (ds_temp(t)) { // process values in t[0] and t[1] to find temperature

        lcd_moveto(0,1);
        char buf[15];
        temperature = ((t[1] << 8) + t[0]);

        final_Main = tempConversion(temperature);
        f_Main = final_Main/10;
        f_dec_Main = final_Main % 10;
        snprintf(buf, 15, "%2d.%1d", f_Main, f_dec_Main);
        lcd_stringout(buf);
        ds_convert();
      }

      if (temp_count > tempMax){
        temp_count = tempMax;
      }

      if (temp_count < tempMin){
        temp_count = tempMin;
      }

      if (count < 0){
        count = 24;
      }

      if (count > 24){
        count = 0;
      }

      if (((PINC) & (1 << PC2)) == 0){ // IF TEMPERATURE THRESHOLD BUTTON IS PRESSED
        button_state = TEMP;
        lcd_moveto(1,12);
        lcd_stringout(" ");
        lcd_moveto(1,4);
        lcd_stringout("<");
      }

      if (((PINC) & (1 << PC4)) == 0){ // IF SOUND ALERT BUTTON IS PRESSED
        button_state = NOTE;
        lcd_moveto(1,4);
        lcd_stringout(" ");
        lcd_moveto(1,12);
        lcd_stringout(">");
      }


    if (changed) { // Did state change?
      eeprom_update_byte((void *) 300, temp_count);
      eeprom_update_byte((void *) 301, count);

	    changed = 0; // Reset changed flag

      if (button_state == TEMP){ // IN TEMPERATURE MODE

        threshold = temp_count;

        if ((getTemperature() - (threshold)) > 2){ // HOT

          TCCR1B &= ~(1 << CS10);
          TCCR1B &= ~(1 << CS11);
          TCCR1B &= ~(1 << CS12);

          PORTD &= ~(1 << PD3);
          PORTD |= (1 << PD2);

          lcd_moveto(0,8);
          lcd_stringout("    ");
          lcd_moveto(0,8);
          lcd_stringout("HOT!");



          unsigned short note_frequency;

          note_frequency = frequency[count]; // Find the frequency of the note
          play_note(note_frequency);  // Call play_note and pass it the frequency
        }

        else if ((getTemperature() - (threshold)) <= 2 && (getTemperature() - (threshold)) > 0 ){ // WARM
          init_timer1(16000000/1024/2);
          PORTD &= ~(1 << PD3);

          lcd_moveto(0,8);
          lcd_stringout("    ");
          lcd_moveto(0,8);
          lcd_stringout("WARM");
        }

        else if (getTemperature() < (threshold))  { // OK

          TCCR1B &= ~(1 << CS10);
          TCCR1B &= ~(1 << CS11);
          TCCR1B &= ~(1 << CS12);

          PORTD &= ~(1 << PD2);
          PORTD |= (1 << PD3);
          lcd_moveto(0,8);
          lcd_stringout("    ");
          lcd_moveto(0,8);
          lcd_stringout("OK");
        }

        lcd_moveto(1,1);
        lcd_stringout("   ");
        lcd_moveto(1,1);
        char buffer[15];
        snprintf(buffer, 15, "%d", temp_count);
        lcd_stringout(buffer);

      }

      if (button_state == NOTE){ // IN SOUND CHANGE MODE

        lcd_moveto(0,14);
        lcd_stringout("  ");
        lcd_moveto(0,14);
        lcd_stringout(notes[count]);

        lcd_moveto(1,14);
        lcd_stringout("  ");
        lcd_moveto(1,14);
        if (count >= 0 && count <= 11){
          lcd_moveto(1,14);
          lcd_stringout("3");
        }
        else if (count > 11 && count <=23){
          lcd_moveto(1,14);
          lcd_stringout("4");
        }
        else {
          lcd_moveto(1,14);
          lcd_stringout("5");
        }
      }
    }
  }
}

void play_note(unsigned short freq) //Play a tone for one second
{

  OCR0A = ((16000000 / 2) / 256)/freq;
  TCCR0B |= (1 << CS02);

}

volatile int buzzer = 0;

void init_timer0()
{
  TCCR0A |= (1 << WGM01);
  TIMSK0 |= (1 << OCIE0A);
}

ISR(TIMER0_COMPA_vect)
{
  buzzer++;

  if (buzzer == frequency[count]){
    TCCR0B &= ~(1 << CS02);
    buzzer = 0;
  }

  PORTB ^= (1 << PB4);

}

ISR(TIMER1_COMPA_vect){
  PORTD ^= (1 << PD2);
}

void variable_delay_us(int delay){ //Delay a variable number of microseconds
    int i = (delay + 5) / 10;

    while (i--)
        _delay_us(10);
}


ISR(PCINT1_vect){
  unsigned char ab = PINC;

  b = ab & (1 << PC1);
  a = ab & (1 << PC5);


  if (button_state == TEMP) {
    if (old_state == 0){
        // Handle A and B inputs for state 0
        if (!b && a){
          new_state = 1;
          temp_count+=1;
        }

        else if (b && !a){
          new_state = 3;
          temp_count-=1;
        }
    }
    else if (old_state == 1){
        // Handle A and B inputs for state 1
        if (b && a){
          new_state = 2;
          temp_count+=1;
        }

        else if (!b && !a){
          new_state = 0;
          temp_count-=1;
        }
    }
    else if (old_state == 2){
        // Handle A and B inputs for state 2
        if (!a){
          new_state = 3;
          temp_count+=1;
        }

        else if (!b && a){
          new_state = 1;
          temp_count-=1;
        }
    }
    else {   // old_state = 3
        // Handle A and B inputs for state 3
        if (!b && !a){
          new_state = 0;
          temp_count+=1;
        }

        else if (b && a){
          new_state = 2;
          temp_count-=1;
        }
    }

    if (new_state != old_state){
        changed = 1;
        old_state = new_state;
    }
  }

  //////////////////////////////////////////////////////////////////////////

  else if (button_state == NOTE){
    if (old_state == 0){
  	    // Handle A and B inputs for state 0
        if (!b && a){
          new_state = 1;
          count+=1;
        }

        else if (b && !a){
          new_state = 3;
          count-=1;
        }
  	}
  	else if (old_state == 1){
  	    // Handle A and B inputs for state 1
        if (b && a){
          new_state = 2;
          count+=1;
        }

        else if (!b && !a){
          new_state = 0;
          count-=1;
        }
  	}
  	else if (old_state == 2){
  	    // Handle A and B inputs for state 2
        if (!a){
          new_state = 3;
          count+=1;
        }

        else if (!b && a){
          new_state = 1;
          count-=1;
        }
  	}
  	else {   // old_state = 3
  	    // Handle A and B inputs for state 3
        if (!b && !a){
          new_state = 0;
          count +=1;
        }

        else if (b && a){
          new_state = 2;
          count -=1;
        }
  	}

    if (new_state != old_state){
  	    changed = 1;
  	    old_state = new_state;
  	}
  }
}

void splashScreen(){  // Splash screen + Initial numbers shown on LCD

  lcd_writecommand(0x01);
  lcd_moveto(0,0);
  lcd_stringout("Matthias Weaser");
  lcd_moveto(1,0);
  lcd_stringout("EE109 Project");

  _delay_ms(1000);
  lcd_writecommand(0x01);

  getTemperature();

  lcd_moveto(1,4);
  lcd_stringout("<");

  lcd_moveto(1,1);
  lcd_stringout("   ");
  lcd_moveto(1,1);
  char buffer[15];
  snprintf(buffer, 15, "%d", temp_count);
  lcd_stringout(buffer);

  lcd_moveto(0,14);
  lcd_stringout(" ");
  lcd_moveto(0,14);
  lcd_stringout(notes[count]);
  lcd_moveto(1,14);
  lcd_stringout("3");

  lcd_moveto(0,8);
  lcd_stringout("OK");

}


int getTemperature(){ // function to extract temperature value from the DS18B20
  if (ds_temp(t)) { // process values in t[0] and t[1] to find temperature

    lcd_moveto(0,1);
    char buf[15];
    temperature = ((t[1] << 8) + t[0]);

    int final;

    final = tempConversion(temperature);
    f = final/10;
    f_dec = final % 10;
    snprintf(buf, 15, "%2d.%1d", f, f_dec);
    lcd_stringout(buf);
    ds_convert();
    return f;


  }
return f;

}


int tempConversion(int in){ // convert temperature into final output value
  int finale = ((in * 9)/8) + 320;
  return finale;
}

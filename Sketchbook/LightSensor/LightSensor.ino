#include <SoftwareSerial.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>

#define TX 1
#define RX 0
#define SLEEP 4 

SoftwareSerial ser(RX, TX);

float readVcc() { 
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  ADMUX = _BV(MUX3) | _BV(MUX2);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring
  return 1126.4 / ((float) ADC); 
}

//Sets the watchdog timer to wake us up, but not reset
//0=16ms, 1=32ms, 2=64ms, 3=128ms, 4=250ms, 5=500ms
//6=1sec, 7=2sec, 8=4sec, 9=8sec
//From: http://interface.khm.de/index.php/lab/experiments/sleep_watchdog_battery/
void setup_watchdog(int timerPrescaler) {

  if (timerPrescaler > 9 ) timerPrescaler = 9; //Limit incoming amount to legal settings

  byte bb = timerPrescaler & 7; 
  if (timerPrescaler > 7) bb |= (1<<5); //Set the special 5th bit if necessary

  //This order of commands is important and cannot be combined
  MCUSR &= ~(1<<WDRF); //Clear the watch dog reset
  WDTCR |= (1<<WDCE) | (1<<WDE); //Set WD_change enable, set WD enable
  WDTCR = bb; //Set new watchdog timeout value
  WDTCR |= _BV(WDIE); //Set the interrupt enable, this will keep unit from resetting after each int
}

void radio_off() {
  digitalWrite(SLEEP, HIGH);
}

void radio_on() {
  digitalWrite(SLEEP, LOW);
  delay(100);
}

void processor_off() {
  ADCSRA &= ~(1<<ADEN); 
  setup_watchdog(6); 
  sleep_enable();
  sleep_mode(); 
  ADCSRA |= (1<<ADEN); 
  delay(100);  
}

void setup() {
  pinMode(SLEEP, OUTPUT);
  digitalWrite(SLEEP, LOW); 
  ser.begin(9600);    
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); //Power down everything, wake up from WDT
  sleep_enable();
  radio_off();
}

ISR(WDT_vect) {
  ser.println("ARF!");
}

void loop() 
{
  static int tick = 0; 
  processor_off();
  tick++;
  if (tick > 10) {
    radio_on(); 
    ser.print("VCC: ");
    ser.println(readVcc());
    radio_off();
    tick = 0;
  }
}


#include <SoftwareSerial.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>

#define TX               9
#define RX              10
#define SLEEP            8
#define ENABLE3V3        7 
#define OPT_101          2
#define APDS_9008        3
#define SLEEP_VOLTAGE  4.7 
#define WAKE_VOLTAGE   4.8 
#define DEBUG_PIN        5


SoftwareSerial ser(RX, TX);

class PowerManager {
public:

  enum PowerState { 
    radio, 
    running, 
    sleeping,
    boot
  }; 
  
  PowerManager() {
    // Setup the 3.3v control pin and keep 3.3v off until we know
    // how much charge we have available...
    pinMode(ENABLE3V3, OUTPUT);
    // Setup the XBEE DTR sleep pin and keep the XBEE asleep
    pinMode(SLEEP, OUTPUT);
    
    // This is how we want to sleep when we do.
    // Power down everything, wake up from WDT
    set_sleep_mode(SLEEP_MODE_PWR_DOWN); 
    sleep_enable();
    setState(boot);
  }
    
  ~PowerManager() {
  }

  PowerState getState() {
    return state; 
  }
  
  void setState(PowerState s) {
    if (s == radio) {
      on3v3();
      onRadio();
    }else if (s == running) {
      on3v3();
      offRadio();
    }else if (s == sleeping) {
      off3v3();
      offRadio();
    }else if (s == boot) {
      off3v3();
      offRadio();
    }
    state = s;
  }

  // Respond to the WDT. Do not call "main" code here. 
  // The snooze() statement in the loop function will 
  // return after this call returns. Determine how much
  // energy we have and what to do about it. 
  // NOTE: using the ADC seems unrelaible in this context, 
  // it seems like I cna't switch input/reference, why?
  void wake() {
  }
  
  // Put the processor to sleep. Power down most units so it's 
  // not running code any longer. This saves TONS of power. Only
  // The watchdog timer can wake us up from here. Anytime it goes
  // off the WDT ISR will run and this function will return.
  void snooze() {
    ADCSRA &= ~(1<<ADEN); 
    
    int prescale;
    // Prescaler values: 0=16ms, 1=32ms, 2=64ms, 3=128ms, 4=250ms, 5=500ms, 6=1sec, 7=2sec, 8=4sec, 9=8sec
    if (state == sleeping) {
      prescale = 9; // wake up as infreqently as possible (8 seconds)
    }else{
      prescale = 6; // wake up every second
    }
    byte bb = prescale & 7; 
    if (prescale > 7) bb |= (1<<5); // Set the special 5th bit if necessary

    // This order of commands is important and cannot be combined
    MCUSR &= ~(1<<WDRF); // Clear the watch dog reset
    WDTCSR |= (1<<WDCE) | (1<<WDE); // Set WD_change enable, set WD enable
    WDTCSR = bb; // Set new watchdog timeout value
    WDTCSR |= _BV(WDIE); //Set the interrupt enable, this will keep unit from resetting after each int

    sleep_enable();

    // Disable ADC
    ADCSRA &= ~(1<<ADEN); 
    
    digitalWrite(DEBUG_PIN, LOW);
    sleep_mode(); 
    digitalWrite(DEBUG_PIN, HIGH);
    
    // Enable ADC
    ADCSRA |= (1<<ADEN); 
    sleep_disable();
    
    updateVcc();
    if ((state == boot || state == sleeping) && supply >= WAKE_VOLTAGE) {
      setState(running);
    }else if (state == boot || state == running || state == radio) {
      if (supply <= SLEEP_VOLTAGE) {
        // If we are in radio this might break the transmission
        // That's okay because we're out of power!
        setState(sleeping);
      }
    }
  }

  // Read VCC assign it to vcc
  float getVcc() {
    return supply;
  }
  
private:
  
  // Turn off XBEE using the DTR pin
  void offRadio() {
    digitalWrite(SLEEP, HIGH);
  }

  // Turn on XBEE using the DTR pin
  void onRadio() {
    digitalWrite(SLEEP, LOW);
    delay(100);
  }

  void off3v3() {
    digitalWrite(ENABLE3V3, LOW);  
  }

  void on3v3() {
    digitalWrite(ENABLE3V3, HIGH);
  }

  // Read VCC assign it to vcc
  void updateVcc() {
    ADMUX = 0x21;
    delay(2);
    ADCSRA |= 1<<ADSC; // Convert
    while (bit_is_set(ADCSRA,ADSC));
    supply = 1126.4 / ((float) ADC); 
  }

  PowerState state;     
  float supply;
};

PowerManager pm; 

ISR(WDT_vect) {
  pm.wake();
}

uint16_t readIR() {
  analogReference(EXTERNAL);
  delay(10);
  return analogRead(OPT_101);
}

uint16_t readVisible() {
  analogReference(EXTERNAL);
  delay(10);
  return analogRead(APDS_9008);
}

void setup() {
  ser.begin(9600);
  pinMode(DEBUG_PIN, OUTPUT);
}

void loop() 
{
  static int tick = 0; 
  static uint32_t odometer = 0; 

  pm.snooze();

  tick++;

  if (pm.getState() == PowerManager::running) {  
    uint16_t IR_reading = readIR();
    uint16_t visible_reading = readVisible();  
    odometer += IR_reading;
    if (tick > 1) {
      pm.setState(PowerManager::radio);
      ser.print("VCC: ");
      ser.print(pm.getVcc());
      ser.print(" IR: ");
      ser.print(IR_reading);
      ser.print(" Visible: ");
      ser.print(visible_reading);
      ser.print(" ODO: ");
      ser.println(odometer);
      pm.setState(PowerManager::running);
      tick = 0;
    }
  }
}


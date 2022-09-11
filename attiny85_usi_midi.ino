/*  ATTINY VARIA MIDI

   Based on :
   ATtiny85 USI UART v3

   David Johnson-Davies - www.technoblogy.com - 6th May 2015
   and Edgar Bonet - www.edgar-bonet.org
   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license:
   http://creativecommons.org/licenses/by/4.0/

   FUSES/Conf : 16Mhz internal
*/
#define CC_CHAN_1                  0xB0
#define CC_CHAN_2                  0xB1
#define CC_CHAN_3                  0xB2
#define CC_CHAN_4                  0xB3
#define CC_CHAN_5                  0xB4
#define CC_CHAN_6                  0xB5
#define CC_CHAN_7                  0xB6
#define CC_CHAN_8                  0xB7
#define CC_CHAN_9                  0xB8
#define NOTEON_CHAN_1              0x90
#define NOTEON_CHAN_2              0x91
#define NOTEON_CHAN_3              0x92
#define NOTEON_CHAN_4              0x93
#define NOTEON_CHAN_5              0x94
#define NOTEON_CHAN_6              0x95
#define NOTEON_CHAN_7              0x96
#define NOTEON_CHAN_8              0x97
#define NOTEON_CHAN_9              0x98

#define FAVOR_PHASE_CORRECT_PWM  0
#define MS_TIMER_TICK_EVERY_X_CYCLES  1

#define DEBOUNCE_DELAY               10 // Debounce time; increase if the output flickers

volatile uint8_t MIDISTATE =         0;
volatile uint8_t MIDIRUNNINGSTATUS = 0;
volatile uint8_t MIDINOTE;
volatile uint8_t MIDIVEL;

volatile uint8_t gpio_switch;
volatile uint8_t gpio_switch_reading;   // reading before debounce
volatile uint8_t last_gpio_state = LOW; // the previous reading from the gpio pin
unsigned long    last_deb_time =     0; // the last time the gpio pin was toggled

volatile int     cv_input;

// Constant
// USI DI
const int DataIn =               PINB0;

// USI UART **********************************************

unsigned char ReverseByte (unsigned char x) {
  x = ((x >> 1) & 0x55) | ((x << 1) & 0xaa);
  x = ((x >> 2) & 0x33) | ((x << 2) & 0xcc);
  x = ((x >> 4) & 0x0f) | ((x << 4) & 0xf0);
  return x;
}

// Initialise USI for UART reception.
void InitialiseUSI (void) {
  DDRB &= ~(1 << DataIn);         // Define DI as input
  USICR = 0;                      // Disable USI.
  GIFR = 1 << PCIF;               // Clear pin change interrupt flag.
  GIMSK |= 1 << PCIE;             // Enable pin change interrupts
  PCMSK |= 1 << PCINT0;           // Enable pin change on pin 0
}

// Pin change interrupt detects start of UART reception.
ISR (PCINT0_vect) {
  if (PINB & 1 << DataIn) return; // Ignore if DI is high
  GIMSK &= ~(1 << PCIE);          // Disable pin change interrupts
  TCCR0A = 2 << WGM00;            // Timer in CTC mode
  TCCR0B = 2 << CS00;             // Set prescaler to /8
  // 16000000/31250 = 512. Avec un prescale de 8, on a 64 par bit.
  OCR0A = 63;                 // Delay (63+1)*8 cycles
  // Ici on veut avoir la moitié du bit, alors on place le counter tel que
  TCNT0 = 226;                // Start counting from (256-32+2)
  // Enable USI OVF interrupt, and select Timer0 compare match as USI Clock source:
  USICR = 1 << USIOIE | 0 << USIWM0 | 1 << USICS0;
  USISR = 1 << USIOIF | 8;        // Clear USI OVF flag, and set counter
}

// USI overflow interrupt indicates we've received a byte
ISR (USI_OVF_vect) {
  USICR = 0;                      // Disable USI
  int rx;
  rx = USIDR;               // Get the data
  rx = ReverseByte(rx);
  GIFR = 1 << PCIF;               // Clear pin change interrupt flag.
  GIMSK |= 1 << PCIE;             // Enable pin change interrupts again

  MIDIParse(rx);
  //digitalWrite(1, !digitalRead(1));
}

void MIDIParse(unsigned char MIDIRX) {
  // Process received byte here
  if ((MIDIRX > 0xBF) && (MIDIRX < 0xF8)) {
    MIDIRUNNINGSTATUS = 0;
    MIDISTATE = 0;
    return;
  }
  if (MIDIRX > 0xF7) return;
  if (MIDIRX & 0x80) {
    MIDIRUNNINGSTATUS = MIDIRX;
    MIDISTATE = 1;
    return;
  }
  if (MIDIRX < 0x80) {
    if (!MIDIRUNNINGSTATUS) return;
    if (MIDISTATE == 1) {
      MIDINOTE = MIDIRX;
      MIDISTATE++;
      return;
    }
  }
  if (MIDISTATE==2 && gpio_switch == HIGH) {
    MIDIVEL=MIDIRX;
    MIDISTATE=1;
// OPTION 1 : Play with CC with Controler 11
    if (MIDIRUNNINGSTATUS==CC_CHAN_1) {
      // Controler 11 == Expression pedal
      if (MIDINOTE == 11){
        // Midi res is 128 -> analogWrite res is 256
        analogWrite(1, MIDIVEL*2);
      }
      if (MIDINOTE == 123){
        analogWrite(1, 0);
      }
    }
// OPTION 2 : Play with NOTE ON
//    if ((MIDIRUNNINGSTATUS==NOTEON_CHAN_1)) {
//      analogWrite(1, MIDINOTE*2);
//      }
//    }
    return;
  }  
}

// Main **********************************************

void setup() {
  // Enable 64 MHz PLL and use as source for Timer1
  PLLCSR = 1 << PCKE | 1 << PLLE;

  // Set up Timer/Counter1 for PWM output
  TIMSK = 0;                     // Timer interrupts OFF
  TCCR1 = 1 << PWM1A | 2 << COM1A0 | 1 << CS10; // PWM A, clear on match, 1:1 prescale

  InitialiseUSI();
  // Setup GPIO
  pinMode(1, OUTPUT); // Enable PWM output pin
  pinMode(2, INPUT);  // CV in
  pinMode(3, INPUT);  // Temp in

  gpio_switch = HIGH;
}

void loop() {
  // Read the switch state
  gpio_switch_reading = digitalRead(2);

  if (gpio_switch_reading != last_gpio_state) {
      // reset the debouncing timer
      last_deb_time = millis();
    }

  if ((millis() - last_deb_time) > DEBOUNCE_DELAY) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (gpio_switch_reading != gpio_switch) {
      gpio_switch = gpio_switch_reading;
    }
  }

  // Act as a MIDI/CV switch
  if (gpio_switch == LOW)
  {
    cv_input = analogRead(2);
    // analogRead res is 1024 -> analogWrite is 256
    analogWrite(1, cv_input/4);
  }
  
  last_gpio_state = gpio_switch_reading;
}

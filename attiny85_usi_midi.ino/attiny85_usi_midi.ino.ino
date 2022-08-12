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
#define FAVOR_PHASE_CORRECT_PWM  0
#define MS_TIMER_TICK_EVERY_X_CYCLES 1

volatile uint8_t MIDISTATE = 0;
volatile uint8_t MIDIRUNNINGSTATUS = 0;
volatile uint8_t MIDINOTE;
volatile uint8_t MIDIVEL;
volatile uint8_t switch_midi_or_cv;
volatile int pot;
volatile int tmp;

// Constant
const int DataIn = PINB0;         // USI DI

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
    }// TODO: pas sûr
  }
  if (MIDISTATE==2 && switch_midi_or_cv == HIGH) {
    MIDIVEL=MIDIRX;
    MIDISTATE=1;
    if ((MIDIRUNNINGSTATUS==0x80)||(MIDIRUNNINGSTATUS==0x90)) {
//      if (MIDINOTE<36) MIDINOTE=36; //If note is lower than C2 set it to C2
//      MIDINOTE=MIDINOTE-36; //Subtract 36 to get into CV range
//      if (MIDINOTE>60) MIDINOTE=60; //If note is higher than C7 set it to C7
        if (MIDIRUNNINGSTATUS == 0x90) { //If note on
//        if (MIDIVEL>0) digitalWrite(2, HIGH); //Set Gate HIGH
//        if (MIDIVEL==0) digitalWrite(2, LOW); //Set Gate LOW
        analogWrite(1, MIDINOTE*2);
      }
//      if (MIDIRUNNINGSTATUS == 0x80) { //If note off
//        digitalWrite(2, LOW); //Set Gate LOW
//      }
    }
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
}

void loop() {
  //digitalWrite(1, !digitalRead(1));
  //delay(500);
  switch_midi_or_cv = digitalRead (2);
  if (switch_midi_or_cv == LOW)
  {
    pot = analogRead(2);
    analogWrite(1, pot/4);
  }
}

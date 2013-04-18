// Timings from AB
const uint8_t LO=4;
const uint8_t HI=15;
const uint8_t MAX=19;
const int PIN=3; // Atmel: OC2B, Arduino: 3
const int tov2 = (1 << TOV2);

inline uint8_t comp(uint8_t b, int bit)
{
   return (b & (1 << bit)) ? HI : LO;
}

inline void sync() {
  while (!(TIFR2 & tov2));
}

// Precondition: the new bit value has been sent
// Compute and send next bit
inline void bang_bit(uint8_t b, int bit)
{
  uint8_t v = comp(b, bit);
  OCR2B = v;
  TIFR2 = tov2;
}

void bang_data(uint8_t *data, int n)
{
  if (n == 0) return;
  
  // Prepare first bit 
  boolean isFirstByte = true;
  uint8_t nextByte = *data;
  uint8_t firstBit = comp(nextByte, 7);
  uint8_t *sentinel = data + n;
  
  // PWM is off, ensure reset delay
  delayMicroseconds(50);
  
  OCR2B = firstBit;
  // Reset counter
  TCNT2 = 0;
  // Time-critical sections start here!
  noInterrupts();
  // Start PWM
  TCCR2A = (1 << COM2B1) | (1 << WGM21) | (1 << WGM20);
  TCCR2B = (1 << WGM22) | (1 << CS20);
  
  // bang the rest of the sequence
  while (true) {
    uint8_t b = nextByte;
    char finished;
    if (isFirstByte) {
      bang_bit(b, 6);
    } else {
      sync();
      bang_bit(b, 7);
      sync();
      bang_bit(b, 6);
    }
    sync();
    bang_bit(b, 5);
    isFirstByte = false;
    sync();
    bang_bit(b, 4);
    data++;
    sync();
    bang_bit(b, 3);
    nextByte = *data;
    sync();
    bang_bit(b, 2);
    // Why so complex, you ask? If this is a simple boolean, the compile
    // will optimize the check to be at the end.
    finished = (data == sentinel) ? 1 : 2;
    sync();
    bang_bit(b, 1);
    sync();
    bang_bit(b, 0);
    // Shave off one cycle: this uses sbrs instead of cp and breq
    if (finished & 1) break;
  }
  // Final bit is in the queue, shut down PWM
  // First, wait for overflow => last bit is now in progress
  sync();
  while (TCNT2 < HI);
  // Reset TCCR2B first: stops the clock
  TCCR2B = 0;
  TCCR2A = 0;
  interrupts();
  // Time-critical section ends
  // Just to be sure
  digitalWrite(PIN, LOW);
}

// Sufficient space for 100 LEDs
uint8_t tape[301];

void setup()
{
  pinMode(PIN, OUTPUT);
  TCCR2A = 0;
  TCCR2B = 0;
  OCR2A = MAX;
  memset(tape, 0, sizeof(tape));
}

// Simple test program to test and check the color order
// Warning: Untested on actual hardware because the computer crashed
uint8_t offset;
void loop()
{
  const int N = 100;
  for (int i = 0; i < N; i++) {
  int r = (i + offset) % 3 == 0;
  int g = (i + offset) % 3 == 1;
  int b = (i + offset) % 3 == 2;
   tape[3*i] = r ? 255 : 0;
   tape[3*i+1] = g ? 255 : 0;
   tape[3*i+2] = b ? 255 : 0;
  }
  bang_data(tape, 3*N);
  delay(500);
  offset++;
}

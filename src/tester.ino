/*
 * Märklin 74460 / k83 Accessory Decoder Batch Tester
 * Emulates Motorola protocol for MC145027DW + HEF4011BT hardware
 * Hardware: ESP32 + L298N Motor Driver (16V DC Power Supply)
 */

const int PIN_ENA = 32;
const int PIN_IN1 = 33;
const int PIN_IN2 = 25;

bool invertTrack = false;

// Base address trits A1..A4 for Märklin Keyboard groups (1 to 16)
const uint8_t GRPS[16][4] = {
  {1,0,0,0}, {2,0,0,0}, {0,1,0,0}, {1,1,0,0}, // Groups 1-4
  {2,1,0,0}, {0,2,0,0}, {1,2,0,0}, {2,2,0,0}, // Groups 5-8
  {0,0,1,0}, {1,0,1,0}, {2,0,1,0}, {0,1,1,0}, // Groups 9-12
  {1,1,1,0}, {2,1,1,0}, {0,2,1,0}, {1,2,1,0}  // Groups 13-16
};

inline void setTrack(bool pulseHigh) {
  bool state = pulseHigh ^ invertTrack;
  if (state) {
    digitalWrite(PIN_IN1, HIGH);
    digitalWrite(PIN_IN2, LOW);
  } else {
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, HIGH);
  }
}

inline void sendZeroBit() {
  setTrack(true);  delayMicroseconds(13);
  setTrack(false); delayMicroseconds(91);
}

inline void sendOneBit() {
  setTrack(true);  delayMicroseconds(91);
  setTrack(false); delayMicroseconds(13);
}

void sendTrit(uint8_t trit) {
  if (trit == 0) {
    sendZeroBit(); sendZeroBit();
  } else if (trit == 1) {
    sendOneBit(); sendOneBit();
  } else {
    sendOneBit(); sendZeroBit();
  }
}

void sendSingleWord(uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4,
                    uint8_t d0, uint8_t d1, uint8_t d2, uint8_t s) {
  sendTrit(a1);
  sendTrit(a2);
  sendTrit(a3);
  sendTrit(a4);
  sendTrit(0);  
  sendTrit(d0);
  sendT1rit(d1);
  sendTrit(d2);
  sendTrit(s);  
}

void sendDoublePacket(uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4,
                      uint8_t d0, uint8_t d1, uint8_t d2, bool activate) {
  uint8_t s = activate ? 1 : 0;
  // MC145027 requires 2 identical words with a strict 624us pause (t1)
  for (int repeat = 0; repeat < 3; repeat++) {
    noInterrupts();
    sendSingleWord(a1, a2, a3, a4, d0, d1, d2, s);
    delayMicroseconds(624); // CRITICAL: t1 pause for MC145027
    sendSingleWord(a1, a2, a3, a4, d0, d1, d2, s);
    interrupts();
    delayMicroseconds(5000); // t2 pause
  }
}

void sendPortCommand(int portNum, bool turnToGreen) {
  int groupIndex = (portNum - 1) / 4;   
  int subPort = (portNum - 1) % 4;      

  uint8_t a1 = GRPS[groupIndex][0];
  uint8_t a2 = GRPS[groupIndex][1];
  uint8_t a3 = GRPS[groupIndex][2];
  uint8_t a4 = GRPS[groupIndex][3];

  uint8_t d1 = (subPort & 0x01) ? 1 : 0;
  uint8_t d2 = (subPort & 0x02) ? 1 : 0;
  uint8_t d0 = turnToGreen ? 1 : 0;

  // L298N single-polarity bridge workaround for old CMOS logic
  // We flip the polarity to ensure the decoder catches the AC-like logic levels
  for (int pol = 0; pol < 2; pol++) {
    invertTrack = (pol == 1);
    setTrack(false); 
    delay(15);       

    sendDoublePacket(a1, a2, a3, a4, d0, d1, d2, true);  
    delay(400); // Pulse duration: 400ms to overcome old spring resistance
    sendDoublePacket(a1, a2, a3, a4, d0, d1, d2, false); 
    delay(50);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_ENA, OUTPUT);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  digitalWrite(PIN_ENA, HIGH);
  setTrack(false);
  Serial.println("Batch Tester Started: Addresses 1-1 to 1-6");
}

void loop() {
  Serial.println("=== Setting all turnouts to STRAIGHT (GREEN) ===");
  for (int i = 1; i <= 6; i++) {
    Serial.printf("Addr 1-%d -> STRAIGHT\n", i);
    sendPortCommand(i, true);
    delay(1000); 
  }
  
  delay(3000); 

  Serial.println("=== Setting all turnouts to TURNOUT (RED) ===");
  for (int i = 1; i <= 6; i++) {
    Serial.printf("Addr 1-%d -> TURNOUT\n", i);
    sendPortCommand(i, false);
    delay(1000); 
  }

  Serial.println("=== Cycle complete. Restarting in 5s ===");
  delay(5000);
}

//
// (C)opyright yoyolb - 2014/08/05
// Version 0.1
// modifs par clox 13/01/2015
// 
// Tested with Arduino UNO and:
//   - RX module RR3-433 (433.92 MHz) => port A1
//   - TX Telecommande Somfy modifiée (433.42 MHz) => port A0
//
//
// Le programme attend la frappe d'un caractère dans la fenètre entrée de la console
// m= montée
// d= descente
// s= my/stop
// p= prog long (pour ajout d'une TC sur un volet)
// q= prog court (pour désinscription d'une TC sur un volet)


#define PORT_TX A0
#define PORT 2   // Arduino UNO = A1

// To store pulse length, in microseconds
volatile word pulse;

// Interrupt handler
#if defined(__AVR_ATmega1280__)
void ext_int_1(void) {
#else
ISR(ANALOG_COMP_vect) {
#endif
    static word last;
    
    // determine the pulse length in microseconds, for either polarity
    pulse = micros() - last;
    last += pulse;
}


// Microseconds
const word k_tempo_wakeup_pulse = 9415;
const word k_tempo_wakeup_silence = 89565;
const word k_tempo_synchro_hw = 2416;
const word k_tempo_synchro_hw_min = 2416*0.7;
const word k_tempo_synchro_hw_max = 2416*1.3;
const word k_tempo_synchro_sw = 4550;
const word k_tempo_synchro_sw_min = 4550*0.7;
const word k_tempo_synchro_sw_max = 4550*1.3;
const word k_tempo_half_symbol = 604;
const word k_tempo_half_symbol_min = 604*0.7;
const word k_tempo_half_symbol_max = 604*1.3;
const word k_tempo_symbol = 1208;
const word k_tempo_symbol_min = 1208*0.7;
const word k_tempo_symbol_max = 1208*1.3;
const word k_tempo_inter_frame_gap = 30415;

class CCodecSomfyRTS {
  public:
    enum t_status {
      k_waiting_synchro,
      k_receiving_data,
      k_complete
    };
    
  public:
    CCodecSomfyRTS();
    t_status pulse(word p);
    bool decode();
    bool transmit(byte cmd, unsigned long rolling_code, byte first);
    
  protected:
    t_status _status;
    byte _cpt_synchro_hw;
    byte _cpt_bits;
    byte _previous_bit;
    bool _waiting_half_symbol;
    byte _payload[7];
};

CCodecSomfyRTS::CCodecSomfyRTS()
: _status(k_waiting_synchro)
, _cpt_synchro_hw(0)
, _cpt_bits(0)
, _previous_bit(0)
, _waiting_half_symbol(false) {
  for(int i=0; i<7; ++i) _payload[i] = 0;
}

CCodecSomfyRTS::t_status CCodecSomfyRTS::pulse(word p) {
  switch(_status) {
    case k_waiting_synchro:
      if (p > k_tempo_synchro_hw_min && p < k_tempo_synchro_hw_max) {
        ++_cpt_synchro_hw;
      }
      else if (p > k_tempo_synchro_sw_min && p < k_tempo_synchro_sw_max && _cpt_synchro_hw >= 4) {
        _cpt_bits = 0;
        _previous_bit = 0;
        _waiting_half_symbol = false;
        for(int i=0; i<7; ++i) _payload[i] = 0;
        _status = k_receiving_data;
      } else {
        _cpt_synchro_hw = 0;
      }
      break;
      
    case k_receiving_data:
      if (p > k_tempo_symbol_min && p < k_tempo_symbol_max && !_waiting_half_symbol) {
        _previous_bit = 1 - _previous_bit;
        _payload[_cpt_bits/8] += _previous_bit << (7 - _cpt_bits%8);
        ++_cpt_bits;
      } else if (p > k_tempo_half_symbol_min && p < k_tempo_half_symbol_max) {
        if (_waiting_half_symbol) {
          _waiting_half_symbol = false;
          _payload[_cpt_bits/8] += _previous_bit << (7 - _cpt_bits%8);
          ++_cpt_bits;
        } else {
          _waiting_half_symbol = true;
        }
      } else {
        _cpt_synchro_hw = 0;
        _status = k_waiting_synchro;
      }
      break;
      
    default:
      Serial.println("Internal error !");
      break;
  }
  
  t_status retval = _status;
  
  if (_status == k_receiving_data && _cpt_bits == 56) {
    retval = k_complete;
    decode();
    _status = k_waiting_synchro;
  }
  
  return retval;
}

bool CCodecSomfyRTS::decode() {
  // Dé-obfuscation
  byte frame[7];
  frame[0] = _payload[0];
  for(int i = 1; i < 7; ++i) frame[i] = _payload[i] ^ _payload[i-1];
   
 for(int i = 0; i < 7; ++i) Serial.print(frame[i], HEX); Serial.print(" ");
  Serial.println("");
  
  // Verification du checksum
  byte cksum = 0;
  for(int i = 0; i < 7; ++i) cksum = cksum ^ frame[i] ^ (frame[i] >> 4);
  cksum = cksum & 0x0F;
  if (cksum != 0) Serial.println("Checksum incorrect !");
  
  // Touche de controle
  switch(frame[1] & 0xF0) {
    case 0x10: Serial.println("My"); break;
    case 0x20: Serial.println("Up"); break;
    case 0x40: Serial.println("Down"); break;
    case 0x80: Serial.println("Prog"); break;
    default: Serial.println("???"); break;
  }
  
  // Rolling code
  unsigned long rolling_code = (frame[2] << 8) + frame[3];
  Serial.print("Rolling code: "); Serial.println(rolling_code);
  
  // Adresse
  unsigned long address = ((unsigned long)frame[4] << 16) + (frame[5] << 8) + frame[6];
  Serial.print("Adresse: "); Serial.println(address, HEX);
}
// ***********************************************************************************
bool CCodecSomfyRTS::transmit(byte cmd, unsigned long rolling_code, byte first) {
  Serial.print("cmd= ");Serial.println(cmd,HEX);
  Serial.print("rc = ");Serial.println(rolling_code,HEX);
  // Construction de la trame claire
  byte data[7];
  data[0] = 0xA7;
  data[1] = cmd << 4;
  data[2] = (rolling_code & 0xFF00) >> 8;
  data[3] = rolling_code & 0x00FF;
  // ******************************** Mettre ici l'adresse de votre TC ou de la TC simulée *************
  data[4] = 0xAB; // Address: USE YOUR OWN ADDRESS !!!
  data[5] = 0xCD; // Address: USE YOUR OWN ADDRESS !!!
  data[6] = 0xEF; // Address: USE YOUR OWN ADDRESS !!!
  
  Serial.print("adr= ");Serial.print (data[4],HEX);Serial.print (data[5],HEX);Serial.println (data[6],HEX);
  
   for(int i = 0; i < 7; ++i) Serial.print(data[i],HEX);
  Serial.println("");
  
  // Calcul du checksum
  byte cksum = 0;
  for(int i = 0; i < 7; ++i) cksum = cksum ^ (data[i]&0xF) ^ (data[i] >> 4);  // ****************
  data[1] = data[1] | (cksum);  // *************************
  
   for(int i = 0; i < 7; ++i) Serial.print(data[i],HEX);
  Serial.println("");
  
  // Obsufscation *****************************
  byte datax[7];
  datax[0]=data[0];
  for(int i = 1; i < 7; ++i) datax[i] = datax[i-1] ^ data[i];  // ********************
  
   for(int i = 0; i < 7; ++i) Serial.print(datax[i],HEX);
  Serial.println("");

  // Emission wakeup, synchro hardware et software 
  digitalWrite(PORT_TX, 1);
  delayMicroseconds(k_tempo_wakeup_pulse);
  digitalWrite(PORT_TX, 0);
  delayMicroseconds(k_tempo_wakeup_silence);
  
  for(int i=0; i<first; ++i) {
    digitalWrite(PORT_TX, 1);
    delayMicroseconds(k_tempo_synchro_hw);
    digitalWrite(PORT_TX, 0);
    delayMicroseconds(k_tempo_synchro_hw);
  }
  
  digitalWrite(PORT_TX, 1);
  delayMicroseconds(k_tempo_synchro_sw);
  digitalWrite(PORT_TX, 0);
  delayMicroseconds(k_tempo_half_symbol);
  
  // Emission des donnees
  for(int i=0; i<56;++i) {
    byte bit_to_transmit = (datax[i/8] >> (7 - i%8)) & 0x01;
    if (bit_to_transmit == 0) {
      digitalWrite(PORT_TX, 1);
      delayMicroseconds(k_tempo_half_symbol);
      digitalWrite(PORT_TX, 0);
      delayMicroseconds(k_tempo_half_symbol);
    }
    else
    {
      digitalWrite(PORT_TX, 0);
      delayMicroseconds(k_tempo_half_symbol);
      digitalWrite(PORT_TX, 1);
      delayMicroseconds(k_tempo_half_symbol);
    }
  }
  
  digitalWrite(PORT_TX, 0);
  delayMicroseconds(k_tempo_inter_frame_gap);
}

int inByte = 0;
byte cmd;
// **************************** Mettre ici la valeur du rolling code à utiliser (valeur du dernier rc +1) *****
unsigned long rc=0001;

void setup () {
    Serial.begin(115200);
    Serial.println("\n[SomfyDecoder]");
    
    pinMode(PORT_TX, OUTPUT);
    digitalWrite(PORT_TX, 0);
    
#if !defined(__AVR_ATmega1280__)
    pinMode(PORT, INPUT);  // use the AIO pin
    digitalWrite(PORT, 1); // enable pull-up

    // use analog comparator to switch at 1.1V bandgap transition
    ACSR = _BV(ACBG) | _BV(ACI) | _BV(ACIE);

    // set ADC mux to the proper port
    ADCSRA &= ~ bit(ADEN);
    ADCSRB |= bit(ACME);
    ADMUX = PORT - 1;
#else
   attachInterrupt(1, ext_int_1, CHANGE);

   DDRE  &= ~_BV(PE5);
   PORTE &= ~_BV(PE5);
#endif
 Serial.println("Fin setup somfy_V2_Em");
}


void loop() {
  static CCodecSomfyRTS codecSomfyRTS;
  static word cpt = 0;
/*  
  cli();
  word p = pulse;
  pulse = 0;
  sei();
*/  
//  codecSomfyRTS.transmit(0x4, XXXX);

if (Serial.available() > 0) {
        char inChar = (char)Serial.read();
        
        if (inChar=='m'){
        cmd=0x02; //***************** Up 
        codecSomfyRTS.transmit(cmd, rc,2);
        int pmax=2;
        for(int p=0; p<pmax;++p) { 
        codecSomfyRTS.transmit(cmd, rc,7);
        }
        
        Serial.print("Montee: RC= ");Serial.println (rc);
        rc++;  
        }

        if (inChar=='d'){
        cmd=0x04;//**************** Down
        codecSomfyRTS.transmit(cmd, rc,2);
        int pmax=2;
        for(int p=0; p<pmax;++p) { 
        codecSomfyRTS.transmit(cmd, rc,7);
        }
        
        Serial.print("Descente: RC= ");Serial.println (rc);
        rc++;   
        }
        
        if (inChar=='s'){
        cmd=0x01;//**************** My/Stop
        codecSomfyRTS.transmit(cmd, rc,2);
        int pmax=2;
        for(int p=0; p<pmax;++p) { 
        codecSomfyRTS.transmit(cmd, rc,7);
        }
        Serial.print("Descente: RC= ");Serial.println (rc);
        rc++;   
        }
        
        if (inChar=='p'){
        cmd=0x08;//**************** Prog
        codecSomfyRTS.transmit(cmd, rc,2);
        int pmax=20;
        for(int p=0; p<pmax;++p) { 
        codecSomfyRTS.transmit(cmd, rc,7);
        }
        Serial.print("Programmation: RC= ");Serial.println (rc);
        rc++;   
        }
        
        if (inChar=='q'){
        cmd=0x08;//**************** Prog
        codecSomfyRTS.transmit(cmd, rc,2);
        int pmax=2;
        for(int p=0; p<pmax;++p) { 
        codecSomfyRTS.transmit(cmd, rc,7);
        }
        Serial.print("Suppression: RC= ");Serial.println (rc);
        rc++;   
        }
}
/*
  if (p != 0) {
    codecSomfyRTS.pulse(p);
  }
*/
}


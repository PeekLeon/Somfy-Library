#include "Arduino.h"
#include "MySomfy.h"

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


MySomfy::MySomfy(uint8_t port_TX)
: _status(k_waiting_synchro)
, _cpt_synchro_hw(0)
, _cpt_bits(0)
, _previous_bit(0)
, _waiting_half_symbol(false) {
	for(int i=0; i<7; ++i) _payload[i] = 0;
	pinMode(port_TX, OUTPUT);
	_port_TX = port_TX;
}

void MySomfy::telecommande(uint8_t adresse_tel1, uint8_t adresse_tel2, uint8_t adresse_tel3)
{
	_adresse_tel1 = adresse_tel1;
	_adresse_tel2 = adresse_tel2;
	_adresse_tel3 = adresse_tel3;
	Serial.print("Telecommande chargee : ");
	if(_adresse_tel1 < 10) 
		Serial.print(0);
	Serial.print(_adresse_tel1, HEX);
	if(_adresse_tel2 < 10) 
		Serial.print(0);
	Serial.print(_adresse_tel2, HEX);
	if(_adresse_tel3 < 10) 
		Serial.print(0);
	Serial.println(_adresse_tel3, HEX);
}

void MySomfy::action(char inChar, int rc_tel)
{
	int rc = rc_tel;
	Serial.print("Adresse : " );
	if(_adresse_tel1 < 10) 
		Serial.print(0);
	Serial.print(_adresse_tel1, HEX);
	if(_adresse_tel2 < 10) 
		Serial.print(0);
	Serial.print(_adresse_tel2, HEX);
	if(_adresse_tel3 < 10) 
		Serial.print(0);
	Serial.print(_adresse_tel3, HEX);
	
	byte cmd ;
	if (inChar=='m'){
        cmd=0x02; //***************** Up 
        transmit(cmd, rc,2);
        int pmax=2;
        for(int p=0; p<pmax;++p) { 
        transmit(cmd, rc,7);
        }
        
        Serial.print(" | Action : Montee    | RC : ");Serial.println (rc);
        }

        if (inChar=='d'){
        cmd=0x04;//**************** Down
        transmit(cmd, rc,2);
        int pmax=2;
        for(int p=0; p<pmax;++p) { 
        transmit(cmd, rc,7);
        }
        
        Serial.print(" | Action : Descendre | RC : ");Serial.println (rc);  
        }
        
        if (inChar=='s'){
        cmd=0x01;//**************** My/Stop
        transmit(cmd, rc,2);
        int pmax=2;
        for(int p=0; p<pmax;++p) { 
        transmit(cmd, rc,7);
        }
        Serial.print(" | Action : Stop      | RC : ");Serial.println (rc);
        }
        
        if (inChar=='p'){
        cmd=0x08;//**************** Prog
        transmit(cmd, rc,2);
        int pmax=20;
        for(int p=0; p<pmax;++p) { 
        transmit(cmd, rc,7);
        }
        Serial.print(" | Action : Program   | RC : ");Serial.println (rc); 
        }
        
        if (inChar=='q'){
        cmd=0x08;//**************** Prog
        transmit(cmd, rc,2);
        int pmax=2;
        for(int p=0; p<pmax;++p) { 
        transmit(cmd, rc,7);
        }
        Serial.print(" | Action : Deprog    | RC : ");Serial.println (rc);
        }
}


MySomfy::t_status MySomfy::pulse(word p) {
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

bool MySomfy::decode() {
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
bool MySomfy::transmit(byte cmd, unsigned long rolling_code, byte first) {
  /*****
  Serial.print("cmd= ");Serial.println(cmd,HEX);
  Serial.print("rc = ");Serial.println(rolling_code,HEX);
  *****/
  // Construction de la trame claire
  byte data[7];
  data[0] = 0xA7;
  data[1] = cmd << 4;
  data[2] = (rolling_code & 0xFF00) >> 8;
  data[3] = rolling_code & 0x00FF;
  // ******************************** Mettre ici l'adresse de votre TC ou de la TC simulée *************
  data[4] = _adresse_tel1; //0xAB; // Address: USE YOUR OWN ADDRESS !!!
  data[5] = _adresse_tel2; //0xCD; // Address: USE YOUR OWN ADDRESS !!!
  data[6] = _adresse_tel3; //0xEF; // Address: USE YOUR OWN ADDRESS !!!
  /***** 
  Serial.print("adr= ");Serial.print (data[4],HEX);Serial.print (data[5],HEX);Serial.println (data[6],HEX);
  
   for(int i = 0; i < 7; ++i) Serial.print(data[i],HEX);
  Serial.println("");
  *****/
  // Calcul du checksum
  byte cksum = 0;
  for(int i = 0; i < 7; ++i) cksum = cksum ^ (data[i]&0xF) ^ (data[i] >> 4);  // ****************
  data[1] = data[1] | (cksum);  // *************************
  
  /*****
   for(int i = 0; i < 7; ++i) Serial.print(data[i],HEX);
  Serial.println("");
  ***/
  
  // Obsufscation *****************************
  byte datax[7];
  datax[0]=data[0];
  for(int i = 1; i < 7; ++i) datax[i] = datax[i-1] ^ data[i];  // ********************
  /*****
   for(int i = 0; i < 7; ++i) Serial.print(datax[i],HEX);
  Serial.println("");
  *****/
  // Emission wakeup, synchro hardware et software 
  digitalWrite(_port_TX, 1);
  delayMicroseconds(k_tempo_wakeup_pulse);
  digitalWrite(_port_TX, 0);
  delayMicroseconds(k_tempo_wakeup_silence);
  
  for(int i=0; i<first; ++i) {
    digitalWrite(_port_TX, 1);
    delayMicroseconds(k_tempo_synchro_hw);
    digitalWrite(_port_TX, 0);
    delayMicroseconds(k_tempo_synchro_hw);
  }
  
  digitalWrite(_port_TX, 1);
  delayMicroseconds(k_tempo_synchro_sw);
  digitalWrite(_port_TX, 0);
  delayMicroseconds(k_tempo_half_symbol);
  
  // Emission des donnees
  for(int i=0; i<56;++i) {
    byte bit_to_transmit = (datax[i/8] >> (7 - i%8)) & 0x01;
    if (bit_to_transmit == 0) {
      digitalWrite(_port_TX, 1);
      delayMicroseconds(k_tempo_half_symbol);
      digitalWrite(_port_TX, 0);
      delayMicroseconds(k_tempo_half_symbol);
    }
    else
    {
      digitalWrite(_port_TX, 0);
      delayMicroseconds(k_tempo_half_symbol);
      digitalWrite(_port_TX, 1);
      delayMicroseconds(k_tempo_half_symbol);
    }
  }
  
  digitalWrite(_port_TX, 0);
  delayMicroseconds(k_tempo_inter_frame_gap);
}



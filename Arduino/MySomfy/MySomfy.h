// Romain LEON - 2016 reprise du code de :
// (C)opyright yoyolb - 2014/08/05
// Version 0.1
// modifs par clox 13/01/2015

#ifndef MySomfy_h
#define MySomfy_h

#include "Arduino.h"
    
class MySomfy
{
	public:
		enum t_status {
		  k_waiting_synchro,
		  k_receiving_data,
		  k_complete
		};
	
	public:
		MySomfy(uint8_t port_TX);
		void telecommande(uint8_t adresse_tel1, uint8_t adresse_tel2, uint8_t adresse_tel3);
		void action(char action, int rc_tel);
		t_status pulse(word p);
		bool decode();
		bool transmit(byte cmd, unsigned long rolling_code, byte first);

	private:
		uint8_t _port_TX;
		uint8_t _adresse_tel1;
		uint8_t _adresse_tel2;
		uint8_t _adresse_tel3;
	
	protected:
		t_status _status;
		byte _cpt_synchro_hw;
		byte _cpt_bits;
		byte _previous_bit;
		bool _waiting_half_symbol;
		byte _payload[7];
};
#endif
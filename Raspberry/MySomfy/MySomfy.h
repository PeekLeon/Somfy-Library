// Romain LEON - 2016 reprise du code de :
// (C)opyright yoyolb - 2014/08/05
// Version 0.1
// modifs par clox 13/01/2015

#ifndef MySomfy_h
#define MySomfy_h

#if defined(ARDUINO) && ARDUINO >= 100
    #include "Arduino.h"
#else
    #include <wiringPi.h>
    #include <stdint.h>
    #define NULL 0
    #define CHANGE 1
#ifdef __cplusplus
extern "C"{
#endif
typedef int boolean;
typedef int byte;

#if !defined(NULL)
#endif
#ifdef __cplusplus
}
#endif
#endif
    
class MySomfy
{
	public:
		MySomfy(int port_TX);
		
		enum t_status {
		  k_waiting_synchro,
		  k_receiving_data,
		  k_complete
		};
		
		void telecommande(int adresse_tel1, int adresse_tel2, int adresse_tel3);
		void action(char action, int rc_tel);
		t_status pulse(int p);
		bool transmit(byte cmd, unsigned long rolling_code, byte first);

	private:
		int _port_TX;
		int _adresse_tel1;
		int _adresse_tel2;
		int _adresse_tel3;
	 
	protected:
		t_status _status;
		byte _cpt_synchro_hw;
		byte _cpt_bits;
		byte _previous_bit;
		bool _waiting_half_symbol;
		byte _payload[7];
};
#endif
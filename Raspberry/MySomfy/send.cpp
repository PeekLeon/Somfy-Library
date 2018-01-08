
#include "MySomfy.h"
//#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
	//*//
	printf(argv[0]);
	printf(argv[1]);
	wiringPiSetup();
	MySomfy somfy(4);
	somfy.telecommande(0xAAFFEE, 0xBB, 0xCC);
	somfy.action('d', 18);
	//*/
	 
	//digitalWrite(4, 1);
	
	//sudo g++ send.cpp MySomfy.cpp -L/usr/local/lib -lwiringPi -o sendTest2

	return 0;
}
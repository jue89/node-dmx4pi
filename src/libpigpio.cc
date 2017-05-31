// -Wall?

#include <nan.h>
#include <node.h>
#include <node_buffer.h>
#include <pigpio.h>


#define BITUS 4


// GLOBAL STATE

int pinTx = -1;
int pinEn = -1;
bool invTx = false;
bool invEn = false;



// PRIVATE METHODS

void terminate() {
	pinTx = -1;
	pinEn = -1;
	gpioTerminate();
}

int clearEn() {
	return gpioWrite( pinEn, invEn ? 1 : 0 );
}

int setEn() {
	return gpioWrite( pinEn, invEn ? 0 : 1 );
}

void setTxPulse( gpioPulse_t *pulse, int bit, int duration ) {
	if( (invTx && bit) || (!invTx && !bit) ) {
		pulse->gpioOn = 0;
		pulse->gpioOff = (1 << pinTx);
	} else {
		pulse->gpioOn = (1 << pinTx);
		pulse->gpioOff = 0;
	}
	pulse->usDelay = duration;
}



// EXPORTED METHODS

NAN_METHOD( init ) {

	int err;

	// Check if module hasn't been initialised yet
	if( pinTx != -1 || pinEn != -1 ) {
		Nan::ThrowError( "pigpio already has been initialised" );
		return;
	}

	// Try to init pigpio
	if( gpioInitialise() < 0 ) {
		Nan::ThrowError( "Could not init pigpio" );
		return;
	}

	// Read Args
	pinTx = info[0]->ToInteger()->Value();
	pinEn = info[1]->ToInteger()->Value();
	invTx = info[2]->ToBoolean()->Value();
	invEn = info[3]->ToBoolean()->Value();

	// TODO: Are both pins the same?

	// Set TX pin
	err = gpioSetMode( pinTx, PI_OUTPUT );
	if( err == PI_BAD_GPIO ) {
		gpioTerminate();
		Nan::ThrowError( "Bad TX pin" );
		return;
	} else if( err == PI_BAD_MODE ) {
		gpioTerminate();
		Nan::ThrowError( "Cannot set TX pin mode" );
		return;
	}

	// Set EN pin
	err = gpioSetMode( pinEn, PI_OUTPUT );
	if( err == PI_BAD_GPIO ) {
		terminate();
		Nan::ThrowError( "Bad EN pin" );
		return;
	} else if( err == PI_BAD_MODE ) {
		terminate();
		Nan::ThrowError( "Cannot set EN pin mode" );
		return;
	}

	clearEn();

}

NAN_METHOD( transmit ) {

	// Check if module has been initialised
	if( pinTx == -1 || pinEn == -1 ) {
		Nan::ThrowError( "pigpio has not been initialised yet" );
		return;
	}

	// Check if a waveform is currently transmitted
	if( gpioWaveTxBusy() ) {
		Nan::ThrowError( "Another transfer hasn't been finished yet" );
		return;
	}

	// Get buffer containing DMX data
	v8::Local<v8::Object> bufferObj = info[0]->ToObject();
	char *bufferData = node::Buffer::Data( bufferObj );
	size_t bufferLength = node::Buffer::Length( bufferObj );

	// Create space for the waveform
	// Pulses: break + mark after break + startbyte
	//         dataLength * (startbit + 8 databits + stopbits)
	size_t pulses = 4 + bufferLength * 10;
	gpioPulse_t *data = (gpioPulse_t*) malloc( pulses * sizeof(gpioPulse_t) );
	gpioPulse_t *ptr = data;

	// Create waveform:
	// 1. Break
	setTxPulse( ptr++, 0, 22 * BITUS );
	// 2. Mark after Break
	setTxPulse( ptr++, 1, 2 * BITUS );
	// 3. Start byte (= 0)
	setTxPulse( ptr++, 0, 9 * BITUS ); // Startbit + 8 0s
	setTxPulse( ptr++, 1, 2 * BITUS ); // Stopbits
	// 4. Data
	for( size_t i = 0; i < bufferLength; i++ ) {
		// Startbit
		setTxPulse( ptr++, 0, BITUS );
		// Databits
		for( int j = 1; j < 256; j = j*2 ) {
			setTxPulse( ptr++, ( bufferData[i] & j ), BITUS );
		}
		// Stopbits
		setTxPulse( ptr++, 1, BITUS * 2 );
	}

	// TODO: Transmit

	// DEBUG: Output everything
	/*printf( "Start: %p\n", data );
	printf( "End:   %p\n", ptr );
	printf( "Size:  %d\n", pulses*sizeof(gpioPulse_t) );
	for( size_t k = 0; k < pulses; k++ ) {
		printf( "%d %d %d\n", data[k].usDelay, data[k].gpioOn, data[k].gpioOff );
	}*/

	free( data );

}

NAN_METHOD( close ) {
	terminate();
}

NAN_MODULE_INIT( define ) {
	NAN_EXPORT( target, init );
	NAN_EXPORT( target, close );
	NAN_EXPORT( target, transmit );
}

NODE_MODULE( libpigpio, define )

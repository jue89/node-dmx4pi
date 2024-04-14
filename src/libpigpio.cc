#include <unistd.h>
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



// ASYNC WORKER
class transmitWorker : public Nan::AsyncWorker {
	private:
		gpioPulse_t *data;
		size_t cnt;

	public:
		transmitWorker( Nan::Callback *callback, gpioPulse_t *data, size_t cnt ) : AsyncWorker( callback ), data( data ), cnt( cnt ) {}

		~transmitWorker() {
			free( data );
		}

		void Execute() {

			int waveId;

			// Set enable line
			setEn();

			// Create waveform
			gpioWaveAddGeneric( this->cnt, this->data );
			waveId = gpioWaveCreate();

			// Transmit everything
			gpioWaveTxSend( waveId, PI_WAVE_MODE_ONE_SHOT );

			// Wait for the transmission to finish
			// First guess required time and then polling
			usleep( ( 22 + 2 + 10 + cnt ) * BITUS );
			while( gpioWaveTxBusy() ) usleep( 10 * BITUS );

			// Clear enable line
			clearEn();

			// Clean up DMA stuff
			gpioWaveDelete( waveId );

		}
};



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
	pinTx = Nan::To<int>(info[0]).FromJust();
	pinEn = Nan::To<int>(info[1]).FromJust();
	invTx = Nan::To<bool>(info[2]).FromJust();
	invEn = Nan::To<bool>(info[3]).FromJust();

	// Are both pins the same?
	if(  pinTx == pinEn ) {
		terminate();
		Nan::ThrowError( "EN and TX cannot be set to the same pin" );
		return;
	}


	// Set TX pin
	err = gpioSetMode( pinTx, PI_OUTPUT );
	if( err == PI_BAD_GPIO ) {
		terminate();
		Nan::ThrowError( "Bad TX pin" );
		return;
	} else if( err == PI_BAD_MODE ) {
		terminate();
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
	v8::Local<v8::Object> bufferObj = Nan::To<v8::Object>(info[0]).ToLocalChecked();
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

	// Get callback method
	Nan::Callback *callback = new Nan::Callback(Nan::To<v8::Function>(info[1]).ToLocalChecked());

	// Transmit waveform asynchronously
	Nan::AsyncQueueWorker( new transmitWorker( callback, data, pulses ) );

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

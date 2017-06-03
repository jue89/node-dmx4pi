# DMX4PI

Convert your Raspberry Pi into a DMX controller. This requires a minimum of additional hardware and utilises the fancy-pancy DMA features of the Raspberry Pi to meet the strict timings of the DMX512 protocol. Under the hood this library uses [pigpio](http://abyz.co.uk/rpi/pigpio/).


## Installation

Beside a running Node v4 or newer you require some dependencies:

Command for Raspbian:

``` shell
sudo apt install build-essential python pigpio
```


## Example + API

Well, calling these few methods *API* is kind of overexaggerated ;)
The following example will show you all methods offered by the API.

``` javascript
// We must run this as root since we are accessing hardware directly.
// Furthermore just one instance of this driver is allowed per Pi concurrently.

// Load and configure the drivers:
const DMX = require( 'dmx4pi' )( {
	// Any free GPIO can be used:
	pinTx: 22,   // Data pin
	pinEn: 27,   // Enable ping
	// The signal levels can be inverted if neccessary:
	invTx: true, // Data pin
	invEn: true  // Enable pin
} );

function fadeDMX( ch3 ) {

	// Stop when channel 3 reached 255
	if( ch3 > 255 ) return;

	// Create buffer that will be transmitted: Ch 1, 2, 4, 5 are static and Ch 3 is faded
	let data = Buffer.from( [ 0, 128, ch3, 0, 0] );

	// DMX.transmit( data ) returns a promise that is fulfilled if the data has been transmitted
	return DMX.transmit( data ).then( () => fadeDMX( ++ch3 ) );

}

// Start fading and shutdown the driver when fading has been finished
fadeDMX( 0 ).then( () => DMX.close() ).catch( console.error );
```


## Hardware

Will be described in the near future.


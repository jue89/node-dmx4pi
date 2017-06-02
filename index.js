'use strict';

const jsonGate = require( 'json-gate' );
const pigpio = require('./build/Release/pigpio.node');

const optSchema = jsonGate.createSchema( {
	type: 'object',
	additionalProperties: false,
	required: true,
	properties: {
		pinTx: { type: 'integer', required: true, minimum: 0, maximum: 53 },
		pinEn: { type: 'integer', required: true, minimum: 0, maximum: 53 },
		invTx: { type: 'boolean', default: false },
		invEn: { type: 'boolean', default: false }
	}
} );

class DMX {

	constructor( opt ) {

		// Process options
		optSchema.validate( opt );

		// Init pins
		// The init method of the native module will make sure only one instance is created successfully
		pigpio.init( opt.pinTx, opt.pinEn, opt.invTx, opt.invEn );

	}

	transmit( data ) { return new Promise( ( resolve ) => {

		// Make sure data is a buffer and has the right size
		if( ! ( data instanceof Buffer ) ) throw new Error( "data must be a Buffer" );
		if( data.length < 1 ) throw new Error( "data must have at least one element" );
		if( data.length > 512 ) throw new Error( "Maximum size of data is 512" );

		// Transmit buffer to driver
		pigpio.transmit( data, resolve );

	} ); }

	close() {

		pigpio.close();

	}

}

module.exports = function( opt ) {
	return new DMX( opt );
}

{
	"targets": [ {
		"target_name": "libpigpio",
		"sources": [ "src/libpigpio.cc" ],
		"include_dirs": [ "<!(node -e \"require('nan')\")" ],
		"libraries": [ "-lpigpio" ]
	} ]
}

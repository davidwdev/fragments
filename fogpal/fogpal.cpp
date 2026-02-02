
/*

MIT License

Copyright (c) 2024-2026 David Walters

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

//=============================================================================


#include <cstdio>
#include <vector>
#include <fstream>
#include <string>

//=============================================================================

struct options_t
{
	std::string strInPaletteFile;
	std::string strOutPaletteFile;
	
	int iSteps = 8;

	uint32_t fogColour = 0;
	
	bool bLastStepEqualsFog = false;
	bool bSplitMode = false;
	bool bRemap = false;
	bool bRemapLab = false;
};

struct color_t
{
	union
	{
		uint32_t value_abgr;
		uint8_t chan[ 4 ]; // R, G, B, A
	};
};

//=============================================================================

//
// print_hello
//
// Print hello text
//
static void print_hello()
{
	printf( "\n---------------------------------------------------------------\n"
			" Palette Fogger (c) David Walters. See LICENSE.txt for details\n"
			"---------------------------------------------------------------\n\n" );
}

//
// print_help
//
// Print help text
//
static void print_help()
{
	// Usage
	printf( " USAGE: fogpal.exe [-?] -col=RRGGBB [-final] -steps=# [-split]\n" );
	printf( "                 [-remap|-remap-lab] -i <palette> <output>\n\n" );

	// Options
	printf( "  -?                This help.\n" );
	printf( "  -col=RRGGBB       The fog colour.\n" );
	printf( "  -final            Make the last line equal to the fog colour.\n" );
	printf( "  -steps=#          Set the number of fog levels to generate.\n" );
	printf( "  -split            Write each fog level to a separate file.\n" );
	printf( "  -remap            Map fog outputs back to original palette.\n" );
	printf( "  -remap-lab        Use Lab color space for remapping.\n" );
	putchar( '\n' );
	printf( "  -i <file>         Filename of input palette.\n" );
	putchar( '\n' );
	printf( "  <output>          Filename of output palette.\n" );
	putchar( '\n' );

	putchar( '\n' );
}

//
// process_args
//
// Process command line arguments
//
static bool process_args( int argc, char** argv, options_t& options )
{
	// Command Line State
	bool bNextArgIsPalette = false;

	// Parse Command Line
	for ( int iarg = 1; iarg < argc; ++iarg ) // skip element[0]
	{
		const char* szArg = argv[ iarg ];

		if ( bNextArgIsPalette )
		{
			bNextArgIsPalette = false;
			options.strInPaletteFile = szArg;
		}
		else if ( strncmp( szArg, "-steps=", 7 ) == 0 )
		{
			options.iSteps = atoi( szArg + 7 );

			if ( options.iSteps <= 1 )
			{
				printf( "Error - invalid number of steps (%d).\n", options.iSteps );
				return false;
			}
		}
		else if ( strncmp( szArg, "-col=", 5 ) == 0 )
		{
			uint32_t colour;
			char* pEnd = nullptr;

			colour = strtoul( szArg + 5, &pEnd, 16 );
			if ( colour >= 0 && colour <= 0xFFFFFF )
			{
				options.fogColour = colour;
			}
			else
			{
				printf( "Error - fog colour is invalid.\n" );
				return false;
			}
		}
		else if ( _stricmp( szArg, "-?" ) == 0 )
		{
			return false;
		}
		else if ( _stricmp( szArg, "-i" ) == 0 )
		{
			bNextArgIsPalette = true;
		}
		else if ( _stricmp( szArg, "-remap" ) == 0 )
		{
			options.bRemap = true;
		}
		else if ( _stricmp( szArg, "-remap-lab" ) == 0 )
		{
			options.bRemap = true;
			options.bRemapLab = true;
		}
		else if ( _stricmp( szArg, "-final" ) == 0 )
		{
			options.bLastStepEqualsFog = true;
		}
		else if ( _stricmp( szArg, "-split" ) == 0 )
		{
			options.bSplitMode = true;
		}
		else if ( options.strOutPaletteFile.empty() && ( szArg[ 0 ] != '-' ) )
		{
			options.strOutPaletteFile = szArg;
		}
		else
		{
			printf( "Error - unknown option \"%s\"\n", szArg );
			return false;
		}

	}; // for each command line argument

	if ( options.strInPaletteFile.empty() )
	{
		printf( "Error - no input file specified.\n" );
		return false;
	}

	if ( options.strOutPaletteFile.empty() )
	{
		printf( "Error - no output file specified.\n" );
		return false;
	}

	if ( options.bSplitMode )
	{
		// Strip file extension

		size_t f, s;
		f = options.strOutPaletteFile.find_last_of( '.', std::string::npos );
		s = options.strOutPaletteFile.find_last_of( "/\\", std::string::npos );

		if ( f != std::string::npos && ( ( f > s ) || ( s == std::string::npos ) ) )
		{
			options.strOutPaletteFile = options.strOutPaletteFile.substr( 0, f );
		}
	}

	return true;
}

double F( double input ) // function f(...), which is used for defining L, a and b changes within [4/29,1]
{
	if ( input > 0.008856 )
	{
		return ( pow( input, 0.333333333 ) ); // maximum 1
	}
	else
	{
		return ( ( 841 / 108 ) * input + 4 / 29 );  //841/108 = 29*29/36*16
	}
}

void XYZtoLab( double X, double Y, double Z, double* L, double* a, double* b )
{
	const double Xo = 244.66128; // reference white
	const double Yo = 255.0;
	const double Zo = 277.63227;
	*L = 116 * F( Y / Yo ) - 16; // maximum L = 100
	*a = 500 * ( F( X / Xo ) - F( Y / Yo ) ); // maximum
	*b = 200 * ( F( Y / Yo ) - F( Z / Zo ) );
}

void RGBtoXYZ( double R, double G, double B, double* X, double* Y, double* Z )
{
	double var_R = R / 255.0;
	double var_G = G / 255.0;
	double var_B = B / 255.0;

	var_R = ( var_R > 0.04045 ) ? pow( ( var_R + 0.055 ) / 1.055, 2.4 )
		: var_R / 12.92;
	var_G = ( var_G > 0.04045 ) ? pow( ( var_G + 0.055 ) / 1.055, 2.4 )
		: var_G / 12.92;
	var_B = ( var_B > 0.04045 ) ? pow( ( var_B + 0.055 ) / 1.055, 2.4 )
		: var_B / 12.92;

	var_R *= 100;
	var_G *= 100;
	var_B *= 100;

	*X = var_R * 0.4124 + var_G * 0.3576 + var_B * 0.1805;
	*Y = var_R * 0.2126 + var_G * 0.7152 + var_B * 0.0722;
	*Z = var_R * 0.0193 + var_G * 0.1192 + var_B * 0.9505;
}

void rgb2lab( int R, int G, int B, double* Lab )
{
	double X, Y, Z;
	RGBtoXYZ( R, G, B, &X, &Y, &Z );
	XYZtoLab( X, Y, Z, &Lab[ 0 ], &Lab[ 1 ], &Lab[ 2 ] );
}

static double color_distance_Lab( color_t* colour1, color_t* colour2 )
{
	double Lab1[ 3 ];
	rgb2lab( colour1->chan[ 0 ], colour1->chan[ 1 ], colour1->chan[ 2 ], Lab1 );

	double Lab2[ 3 ];
	rgb2lab( colour2->chan[ 0 ], colour2->chan[ 1 ], colour2->chan[ 2 ], Lab2 );

	double x;
	double delta = 0;

	x = Lab1[ 0 ] - Lab2[ 0 ];
	delta += x * x;

	x = Lab1[ 1 ] - Lab2[ 1 ];
	delta += x * x;

	x = Lab1[ 2 ] - Lab2[ 2 ];
	delta += x * x;

	return delta;
}

//
// remap_Lab
//
// Return the closest color in the base palette. Use Lab color space comparison.
//
static uint32_t remap_Lab( std::vector< uint32_t >& aPalette, const size_t baseSize, const uint32_t input )
{
	size_t best_index = 0;
	double score, best_score;

	best_score = color_distance_Lab( (color_t*)&input, (color_t*)&( aPalette[ 0 ] ) );

	for ( size_t i = 1; i < baseSize; ++i )
	{
		score = color_distance_Lab( (color_t*)&input, (color_t*)&( aPalette[ i ] ) );

		if ( score <= best_score )
		{
			best_score = score;
			best_index = i;
		}
	}

	return aPalette[ best_index ];
}

static double color_distance_rgb( color_t* colour1, color_t* colour2 )
{
	double x;
	double delta;

	x = double( colour1->chan[ 0 ] ) - double( colour2->chan[ 0 ] );
	delta = x * x;

	x = double( colour1->chan[ 1 ] ) - double( colour2->chan[ 1 ] );
	delta += x * x;

	x = double( colour1->chan[ 2 ] ) - double( colour2->chan[ 2 ] );
	delta += x * x;

	return delta;
}

//
// remap_rgb
//
// Return the closest color in the base palette. Use rgb color space comparison.
//
static uint32_t remap_rgb( std::vector< uint32_t >& aPalette, const size_t baseSize, const uint32_t input )
{
	size_t best_index = 0;
	double score, best_score;

	best_score = color_distance_rgb( (color_t*)&input, (color_t*)&( aPalette[ 0 ] ) );

	for ( size_t i = 1; i < baseSize; ++i )
	{
		score = color_distance_rgb( (color_t*)&input, (color_t*)&( aPalette[ i ] ) );

		if ( score <= best_score )
		{
			best_score = score;
			best_index = i;
		}
	}

	return aPalette[ best_index ];
}

//
// generate_fog
//
// Generate additional palette entries corresponding to equally spaced steps.
//
static void generate_fog( std::vector< uint32_t >& aPalette, const options_t& options )
{
	// cache the base size of the raw palette (fog step 0)
	const size_t baseSize = aPalette.size();

	float fScale;
	if ( options.bLastStepEqualsFog )
	{
		fScale = 1.0f / static_cast<float>( options.iSteps - 1 );
	}
	else
	{
		fScale = 1.0f / static_cast<float>( options.iSteps );
	}

	const float target_r = static_cast<float>( ( options.fogColour >> 16 ) & 0xFF );
	const float target_g = static_cast<float>( ( options.fogColour >> 8 ) & 0xFF );
	const float target_b = static_cast<float>( ( options.fogColour ) & 0xFF );

	for ( int iStep = 1; iStep < options.iSteps; ++iStep )
	{
		// how much fog at this step?
		const float fFog = static_cast<float>( iStep ) * fScale;

		for ( size_t i = 0; i < baseSize; ++i )
		{
 			// get the raw colour
 			const uint32_t rawColour = aPalette[ i ];

			// ... convert to float
			float source_r = static_cast<float>( ( rawColour >> 16 ) & 0xFF );
			float source_g = static_cast<float>( ( rawColour >> 8 ) & 0xFF );
			float source_b = static_cast<float>( ( rawColour ) & 0xFF );

			// ... blend
			float r, g, b;
			r = source_r * ( 1 - fFog ) + target_r * ( fFog );
			g = source_g * ( 1 - fFog ) + target_g * ( fFog );
			b = source_b * ( 1 - fFog ) + target_b * ( fFog );

			// generate the fogged value.
			uint32_t fogOutput = 0;
			fogOutput |= std::min( std::max( static_cast<int>( r ), 0 ), 255 ) << 16;
			fogOutput |= std::min( std::max( static_cast<int>( g ), 0 ), 255 ) << 8;
			fogOutput |= std::min( std::max( static_cast<int>( b ), 0 ), 255 );

			// remap?
			if ( options.bRemapLab )
			{
				fogOutput = remap_Lab( aPalette, baseSize, fogOutput );
			}
			else if ( options.bRemap )
			{
				fogOutput = remap_rgb( aPalette, baseSize, fogOutput );
			}

			// add to the array
			aPalette.push_back( fogOutput );
		}
	}
}

//
// parse_hexfile
//
// Parse a .hex file output by "aseprite". A simple list of \n separated 6 byte ASCII hex values.
//
static void parse_hexfile( std::ifstream& fileInput, std::vector<uint32_t>& aPalette )
{
	while ( !fileInput.eof() )
	{
		std::string line;
		std::getline( fileInput, line );

		if ( line.length() != 6 )
		{
			break;
		}

		char* pEnd = nullptr;
		uint32_t colour = std::strtoul( line.c_str(), &pEnd, 16 );

		if ( pEnd == line.c_str() )
		{
			break;
		}

		aPalette.push_back( colour );

		// printf( "debug: %s -> %06x\n", line.c_str(), colour );
	}
}

//
// write_hexfile
//
// Dump the whole palette to disk in .hex format.
//
static bool write_hexfile( std::vector< uint32_t >& aPalette, size_t iBase, size_t iCount, const std::string& strFileName )
{
	char buf[ 64 ];

	printf( "Writing \"%s\" ... ", strFileName.c_str() );

	std::ofstream file( strFileName );
	if ( file.is_open() )
	{
		for ( size_t i = 0; i < iCount; ++i )
		{
			const uint32_t c = aPalette[ i + iBase ];

			// .hex palette file format
			sprintf_s( buf, sizeof( buf ), "%06x\n", c );

			file << buf;
		}

		file.close();
		printf( "OK\n" );
		return true;
	}
	else
	{
		printf( "FAILED\n" );
		return false;
	}
}

//
// do_work
//
// Palette fogger tool.
//
static void do_work( const options_t& options )
{
	print_hello();

	printf( "Loading palette \"%s\" ... ", options.strInPaletteFile.c_str() );

	// Load input palette file - keep it open to prevent common user error of overwriting input!
	std::ifstream fileInput( options.strInPaletteFile );
	if ( fileInput.is_open() )
	{
		//
	}
	else
	{
		printf( "FAILED\n\n" );
		return;
	}

	std::vector< uint32_t > aPalette;
	parse_hexfile( fileInput, aPalette );

	// Usable palette?
	if ( aPalette.empty() )
	{
		printf( "INVALID\n\n" );
		return;
	}
	else
	{
		printf( "OK\n\n" );
	}

	const size_t initialSize = aPalette.size();

	// Explain what we're doing.
	printf( "Generating %d steps of fog (#%06x) for this palette.\n", options.iSteps, options.fogColour );

	if ( options.bLastStepEqualsFog )
	{
		printf( "The last %llu entries will equal the fog colour (#%06x).\n", initialSize, options.fogColour );
	}

	printf( "\nThe palette is now %llu x %u = %llu entries.\n\n", initialSize, options.iSteps, initialSize * options.iSteps );

	generate_fog( aPalette, options );

	// Try and write the output.

	if ( options.bSplitMode )
	{
		printf( "Writing split palette files ...\n\n" );

		for ( int step = 1; step < options.iSteps; ++step )
		{
			std::string file;
			file = options.strOutPaletteFile;
			file += "_";
			file += std::to_string( step );
			file += ".hex";

			if ( write_hexfile( aPalette, step * initialSize, initialSize, file ) == false )
			{
				break;
			}
		}
	}
	else
	{
		write_hexfile( aPalette, 0, aPalette.size(), options.strOutPaletteFile );
	}

	// Done. We can close the input now.
	fileInput.close();
}

//
// main
//
// Program entry point.
//
int main( int argc, char** argv )
{
	// Options
	options_t options;

	if ( process_args( argc, argv, options ) )
	{
		do_work( options );
	}
	else
	{
		// Failure. Offer the user some help.
		print_hello();
		print_help();
	}

	return 0;
}

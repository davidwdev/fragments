
/*

MIT License

Copyright (c) 2026 David Walters

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

#include <algorithm>
#include <set>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <direct.h>
#include <io.h>

#include "png.h" // libpng

#define STBI_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//=============================================================================

struct dither_t
{
	uint8_t index;

	float err_r;
	float err_g;
	float err_b;
};

struct dithermap_t
{

public:

	dither_t* _data_ptr = nullptr;
	size_t _width = 0;
	size_t _height = 0;

public:

	void Create( size_t w, size_t h )
	{
		_width = w;
		_height = h;
		_data_ptr = new dither_t[ w * h + 1 ]; // +1 !
	}

	dither_t& Element( int x, int y )
	{
		return _data_ptr[ x + y * _width ];
	}

	const dither_t& Element( int x, int y ) const
	{
		return _data_ptr[ x + y * _width ];
	}
};

struct color_t
{
	union
	{
		uint32_t value_abgr;
		uint8_t chan[ 4 ]; // R, G, B, A
	};

public:

	uint32_t BGR() const { return value_abgr & 0xFFFFFF; }

	void FromDither( dither_t& dither )
	{
		chan[ 0 ] = static_cast<uint8_t>( std::clamp( static_cast<int>( std::floorf( dither.err_r * 255.0f ) ), 0, 255 ) );
		chan[ 1 ] = static_cast<uint8_t>( std::clamp( static_cast<int>( std::floorf( dither.err_g * 255.0f ) ), 0, 255 ) );
		chan[ 2 ] = static_cast<uint8_t>( std::clamp( static_cast<int>( std::floorf( dither.err_b * 255.0f ) ), 0, 255 ) );
		chan[ 3 ] = 0xFF;
	}
};

struct fcolor_t
{
	float chan[ 4 ]; // R, G, B, A

public:

	static fcolor_t Blend( fcolor_t a, fcolor_t b, float t )
	{
		fcolor_t out;
		for ( int i = 0; i < 4; ++i )
		{
			out.chan[ i ] = ( a.chan[ i ] * ( 1 - t ) ) + ( b.chan[ i ] * t );
		}
		return out;
	}

	static fcolor_t Blend( color_t a, color_t b, float t )
	{
		fcolor_t out;
		for ( int i = 0; i < 4; ++i )
		{
			out.chan[ i ] = ( a.chan[ i ] * ( 1 - t ) ) + ( b.chan[ i ] * t );
		}
		return out;
	}
};

struct colormap_t
{

public:

	color_t* _data_ptr = nullptr;
	size_t _width = 0;
	size_t _height = 0;

public:

	void Create( size_t w, size_t h )
	{
		_width = w;
		_height = h;
		_data_ptr = new color_t[ w * h ];
	}

	void CopyFromRGB( uint8_t* src )
	{
		color_t* pout = _data_ptr;
		color_t* pend = _data_ptr + _width * _height;
		while ( pout < pend )
		{
			pout->chan[ 0 ] = *src++;
			pout->chan[ 1 ] = *src++;
			pout->chan[ 2 ] = *src++;
			pout->chan[ 3 ] = 0xFF;
			++pout;
		}
	}

	void CopyFromRGBA( uint8_t* src )
	{
		color_t* pout = _data_ptr;
		color_t* pend = _data_ptr + _width * _height;
		while ( pout < pend )
		{
			pout->chan[ 0 ] = *src++;
			pout->chan[ 1 ] = *src++;
			pout->chan[ 2 ] = *src++;
			pout->chan[ 3 ] = *src++;
			++pout;
		}
	}

	void Plot( int x, int y, color_t value )
	{
		_data_ptr[ x + y * _width ] = value;
	}

	color_t Peek( int x, int y ) const
	{
		return _data_ptr[ x + y * _width ];
	}

	color_t PeekClamp( int x, int y ) const
	{
		if ( x < 0 ) x = 0;
		else if ( x >= _width ) x = (int)_width - 1;

		if ( y < 0 ) y = 0;
		else if ( y >= _height ) y = (int)_height - 1;

		return _data_ptr[ x + y * _width ];
	}
};

struct indexmap_t
{

public:

	uint8_t* _data_ptr = nullptr;
	int _width = 0;
	int _height = 0;
	uint32_t _uStride = 0;
	uint32_t _uBPP = 0;
	uint32_t _uPixelsPerByte = 0;

public:

	void Create( int w, int h, uint32_t bpp )
	{
		_width = w;
		_height = h;
		_uBPP = bpp;

		_uPixelsPerByte = 8 / _uBPP;
		_uStride = ( ( w + ( _uPixelsPerByte - 1 ) ) / _uPixelsPerByte );
		const uint32_t payload = _uStride * _height;
		_data_ptr = new uint8_t[ payload ];
	}

	void Plot( int x, int y, uint8_t value )
	{
		const int byte_x = x / _uPixelsPerByte; // which byte are we editing?
		const int frac_x = x - ( byte_x * _uPixelsPerByte ); // which pixel in the byte?
		const int shift = ( _uPixelsPerByte - 1 - frac_x ) * _uBPP;

		uint8_t* byte_ptr = _data_ptr + ( (size_t)y * _uStride ) + byte_x;

		switch ( _uBPP )
		{

		case 1:
			{
				const uint8_t mask = 0x01 << shift;
				*byte_ptr = ( *byte_ptr & ~mask ) | ( ( value & 0x01 ) << shift );
			}
			break;

		case 2:
			{
				const uint8_t mask = 0x03 << shift;
				*byte_ptr = ( *byte_ptr & ~mask ) | ( ( value & 0x03 ) << shift );
			}
			break;

		case 4:
			{
				const uint8_t mask = 0x0F << shift;
				*byte_ptr = ( *byte_ptr & ~mask ) | ( ( value & 0x0F ) << shift );
			}
			break;

		case 8:
			{
				*byte_ptr = value; // just set!
			}
			break;

		}; // switch ( uBPP )
	}

};

//=============================================================================

static void png_write_data_fn( png_structp png_ptr, png_bytep p_data, png_size_t size )
{
	// Get our FILE pointer, and write data to it.
	FILE* fp = reinterpret_cast<FILE*>( png_get_io_ptr( png_ptr ) );
	fwrite( p_data, 1, size, fp );
}

static void png_flush_data_fn( png_structp png_ptr )
{
	// stub
}

static void png_error_fn( png_structp png_ptr, png_const_charp error_message )
{
	printf( "png error: %s\n", error_message );
	png_longjmp( png_ptr, -1 );
}

static void png_warn_fn( png_structp png_ptr, png_const_charp error_message )
{
	printf( "png warning: %s\n", error_message );
}

//=============================================================================

static int rgb_color_distance_squared( color_t colour1, color_t colour2 )
{
	int x;
	int delta;

	x = static_cast<int>( colour1.chan[ 0 ] ) - static_cast<int>( colour2.chan[ 0 ] );
	delta = x * x;

	x = static_cast<int>( colour1.chan[ 1 ] ) - static_cast<int>( colour2.chan[ 1 ] );
	delta += x * x;

	x = static_cast<int>( colour1.chan[ 2 ] ) - static_cast<int>( colour2.chan[ 2 ] );
	delta += x * x;

	return delta;
}

static uint8_t find_nearest_palette_index( const color_t& colour1, std::vector< color_t >& aPalette )
{
	size_t best_index = 0;
	int best_score = rgb_color_distance_squared( colour1, aPalette[ 0 ] );

	for ( size_t i = 1; i < aPalette.size(); ++i )
	{
		int score = rgb_color_distance_squared( colour1, aPalette[ i ] );

		if ( score <= best_score )
		{
			best_score = score;
			best_index = i;
		}
	}

	return (uint8_t)best_index;
}

typedef enum
{
	FILTER_NEAREST,
	FILTER_BILINEAR,
}
filter_t;

struct options_t
{
	size_t width = 0;
	size_t height = 0;
	bool aspect_preserve = false;

	filter_t filter = FILTER_NEAREST;

	std::string strPaletteFile;
	std::vector< color_t > aPalette;

	std::set< std::string > aInputFiles;

	bool bDither = false;

	std::string strOutFile;
	std::string strOutFolder;
};

//=============================================================================

//
// print_hello
//
// Print hello text
//
static void print_hello()
{
	printf( "\n------------------------------------------------------------------------------\n"
			" Resize and Palettize an Image (c) David Walters. See LICENSE.txt for details\n"
			"------------------------------------------------------------------------------\n\n" );
}

//
// print_help
//
// Print help text
//
static void print_help()
{
	// Usage
	printf( " USAGE: imgsize.exe [-?] -w <width> -h <height> -aspect [-pal <palette> [-dither]]\n" );
	printf( "                    [-nearest|-bilinear]\n" );
	printf( "                    <image>[...] [-o <image>]|[-outdir <folder>]\n" );
	putchar( '\n' );

	// Options
	printf( "  -?                 This help.\n" );

	putchar( '\n' );
	printf( "  -w <width>         Output width in pixels.\n" );
	printf( "  -h <height>        Output height in pixels.\n" );
	printf( "  -aspect            Preserve aspect ratio if either width or height is omitted.\n" );

	putchar( '\n' );
	printf( "  -pal <palette>     Palette file to apply (in .HEX format)\n" );
	printf( "  -dither            Apply error-diffusion dithering when using a palette.\n" );

	putchar( '\n' );
	printf( "  -nearest           Filter mode: Nearest [default]\n" );
	printf( "  -bilinear          Filter mode: Bilinear\n" );

	putchar( '\n' );
	printf( "  <image>[...]       Add image(s) to the processing list. Wildcards supported.\n" );

	putchar( '\n' );
	printf( "  -o <file>          Specify an output file. Not supported with multiple images.\n" );
	printf( "  -outdir <folder>   Specify an output folder. Ignored if -o is used.\n" );

	putchar( '\n' );
	putchar( '\n' );
}

//
// load_palette
//
// Parse a .hex file output by "aseprite". A simple list of \n separated 6 byte ASCII hex values.
//
static bool load_palette( const char* szFileName, std::vector<color_t>& aPalette )
{
	// Load input palette file - keep it open to prevent common user error of overwriting input!
	std::ifstream fileInput( szFileName );
	if ( fileInput.is_open() == false )
	{
		return false;
	}

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

		color_t c;
		c.chan[ 0 ] = ( colour >> 16 ) & 0xFF; // RED
		c.chan[ 1 ] = ( colour >> 8 ) & 0xFF; // GREEN
		c.chan[ 2 ] = ( colour ) & 0xFF; // BLUE
		c.chan[ 3 ] = 0xFF; // ALPHA

		aPalette.push_back( c );
	}

	// Usable palette?
	return ( aPalette.size() >= 2 );
}

//
// add_files_wildcard
//
// Add a file to an array. Supports wildcards like *.png, so multiple files may be added.
//
static void add_files_wildcard( const char* szWildCard, std::set< std::string >& aFiles )
{
	// printf( "scan input %s\n", szWildCard );

	// split argument into path and file.
	std::string path = szWildCard;
	size_t slash_find = path.find_last_of( "/\\", path.npos ); // either kind of path separators
	if ( slash_find == path.npos )
	{
		path.clear();
	}
	else
	{
		path = path.substr( 0, slash_find + 1 );
		// printf( "from path %s\n", path.c_str() );
	}

	struct _finddata_t fileinfo;

	// Start the search
	intptr_t handle = _findfirst( szWildCard, &fileinfo );
	if ( handle != -1L )
	{
		do
		{
			// A file, not a sub-directory?
			if ( !( fileinfo.attrib & _A_SUBDIR ) )
			{
				std::string filepath;
				filepath = path + fileinfo.name;

				// printf( "found %s\n", filepath.c_str() );

				aFiles.insert( filepath );
			}
		}
		while ( _findnext( handle, &fileinfo ) == 0 ); // Get the next one

		_findclose( handle );
	}
}

//
// process_args
//
// Process command line arguments
//
static bool process_args( int argc, char** argv, options_t& options )
{
	// Command Line State
	bool bNextArgIsWidth = false;
	bool bNextArgIsHeight = false;
	bool bNextArgIsPalette = false;
	bool bNextArgIsOutFile = false;
	bool bNextArgIsOutFolder = false;

	// Parse Command Line
	for ( int iarg = 1; iarg < argc; ++iarg ) // skip element[0]
	{
		const char* szArg = argv[ iarg ];

		if ( bNextArgIsWidth )
		{
			bNextArgIsWidth = false;
			int w = atol( szArg );
			if ( w <= 0 )
			{
				std::cout << "Error - invalid width \"" << w << "\"";
				return false;
			}
			else
			{
				options.width = uint32_t( w );
			}
		}
		else if ( bNextArgIsHeight )
		{
			bNextArgIsHeight = false;
			int h = atol( szArg );
			if ( h <= 0 )
			{
				std::cout << "Error - invalid height \"" << h << "\"";
				return false;
			}
			else
			{
				options.height = uint32_t( h );
			}
		}
		else if ( bNextArgIsOutFile )
		{
			bNextArgIsOutFile = false;
			options.strOutFile = szArg;
		}
		else if ( bNextArgIsOutFolder )
		{
			bNextArgIsOutFolder = false;
			options.strOutFolder = szArg;
		}
		else if ( bNextArgIsPalette )
		{
			bNextArgIsPalette = false;

			bool bFailed = false;

			if ( load_palette( szArg, options.aPalette ) == false )
			{
				std::cout << "Error - failed to load palette from \"" << szArg << "\"";
				bFailed = true;
			}

			if ( bFailed == false )
			{
				if ( options.aPalette.size() < 2 )
				{
					std::cout << "Error - the palette loaded from \"" << szArg << "\" is too small (" << options.aPalette.size() << " entries).\n";
					bFailed = true;
				}
				else if ( options.aPalette.size() > 256 )
				{
					std::cout << "Error - the palette loaded from \"" << szArg << "\" has over 256 entries (" << options.aPalette.size() << ") and is too big.\n";
					bFailed = true;
				}
			}

			if ( bFailed )
			{
				return false;
			}
			else
			{
				options.strPaletteFile = szArg;
			}
		}
		else if ( _stricmp( szArg, "-?" ) == 0 )
		{
			return false;
		}
		else if ( _stricmp( szArg, "-aspect" ) == 0 )
		{
			options.aspect_preserve = true;
		}
		else if ( _stricmp( szArg, "-nearest" ) == 0 )
		{
			options.filter = FILTER_NEAREST;
		}
		else if ( _stricmp( szArg, "-bilinear" ) == 0 )
		{
			options.filter = FILTER_BILINEAR;
		}
		else if ( _stricmp( szArg, "-w" ) == 0 )
		{
			bNextArgIsWidth = true;
		}
		else if ( _stricmp( szArg, "-h" ) == 0 )
		{
			bNextArgIsHeight = true;
		}
		else if ( _stricmp( szArg, "-pal" ) == 0 )
		{
			bNextArgIsPalette = true;
		}
		else if ( _stricmp( szArg, "-o" ) == 0 )
		{
			bNextArgIsOutFile = true;
		}
		else if ( _stricmp( szArg, "-outdir" ) == 0 )
		{
			bNextArgIsOutFolder = true;
		}
		else if ( _stricmp( szArg, "-dither" ) == 0 )
		{
			options.bDither = true;
		}
		else
		{
			// assume it's input files.
			add_files_wildcard( szArg, options.aInputFiles );
		}

	}; // for each command line argument

	if ( options.aInputFiles.empty() )
	{
		std::cout << "Error - no input file(s) specified.\n";
		return false;
	}

	if ( options.width == 0 && !( options.height && options.aspect_preserve ) )
	{
		std::cout << "Error - no output width was specified.\n";
		return false;
	}

	if ( options.height == 0 && !( options.width && options.aspect_preserve ) )
	{
		std::cout << "Error - no output height was specified.\n";
		return false;
	}

	return true;
}

//==============================================================================

static bool make_path( std::string& path )
{
	int e, er;
	int start = 0;

	// Skip drive?
	if ( ( path.size() >= 2 ) && ( path[ 1 ] == ':' ) )
	{
		start = 3;
	}

	// Create tree
	for ( size_t i = start; i < path.size(); ++i )
	{
		if ( path[ i ] == '\\' || path[ i ] == '/' )
		{
			// Briefly turn into a nul terminated c-string
			path[ i ] = '\0';
			e = _mkdir( path.c_str() );
			er = errno;
			path[ i ] = '\\';

			if ( e != 0 && er != EEXIST )
			{
				return false;
			}
		}
	}

	// Create full directory
	e = _mkdir( path.c_str() );
	er = errno;
	return !( e != 0 && er != EEXIST );
}

void write_png_rgb24( const colormap_t& image, const std::string& strOutFile )
{
	// Open
	std::cout << "Writing \"" << strOutFile << "\" (RGB/24) ... ";

	FILE* fp = nullptr;
	int e = fopen_s( &fp, strOutFile.c_str(), "wb" );
	if ( e != 0 || fp == nullptr )
	{
		std::cout << "ERROR (attempted overwrite?)\n\n";
		return;
	}

	// Initialise the PNG.
	png_structp png_ptr;
	png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, nullptr, png_error_fn, png_warn_fn );
	if ( png_ptr == nullptr )
	{
		std::cout << "ERROR: png_create_write_struct failed.\n";
		return;
	}

	// Initialise the information structure.
	png_infop info_ptr;
	info_ptr = png_create_info_struct( png_ptr );
	if ( info_ptr == nullptr )
	{
		std::cout << "ERROR: png_create_info_struct failed.\n";
		return;
	}

	jmp_buf* p_jmp_buf = png_set_longjmp_fn( png_ptr, longjmp, sizeof( jmp_buf ) );

	if ( setjmp( *p_jmp_buf ) != -1 )
	{
		// Setup the writer
		png_set_write_fn( png_ptr, fp, png_write_data_fn, png_flush_data_fn );

		// Setup the header
		png_set_IHDR( png_ptr, info_ptr, uint32_t( image._width ), uint32_t( image._height ),
					  8 /*CHANNEL DEPTH*/, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
					  PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE );

		// Write!
		png_write_info( png_ptr, info_ptr );

		png_bytep row = (png_bytep)malloc( image._width * 3 );

		for ( int i = 0; i < image._height; ++i )
		{
			png_bytep src = (png_bytep)( image._data_ptr + ( i * image._width ) );
			png_bytep dst = row;
			for ( size_t x = 0; x < image._width; ++x )
			{
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
				*src++; // skip alpha
			}

			png_write_rows( png_ptr, &row, 1 );
		}

		free( row );

		png_write_end( png_ptr, nullptr );

		std::cout << "OK\n";
	}

	// Destroy the main writer and info structures
	png_destroy_write_struct( &png_ptr, &info_ptr );

	fclose( fp );
}

void write_png_rgb32( const colormap_t& image, const std::string& strOutFile )
{
	// Open
	std::cout << "Writing \"" << strOutFile << "\" (RGB/32) ... ";

	FILE* fp = nullptr;
	int e = fopen_s( &fp, strOutFile.c_str(), "wb" );
	if ( e != 0 || fp == nullptr )
	{
		std::cout << "ERROR (attempted overwrite?)\n\n";
		return;
	}

	// Initialise the PNG.
	png_structp png_ptr;
	png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, nullptr, png_error_fn, png_warn_fn );
	if ( png_ptr == nullptr )
	{
		std::cout << "ERROR: png_create_write_struct failed.\n";
		return;
	}

	// Initialise the information structure.
	png_infop info_ptr;
	info_ptr = png_create_info_struct( png_ptr );
	if ( info_ptr == nullptr )
	{
		std::cout << "ERROR: png_create_info_struct failed.\n";
		return;
	}

	jmp_buf* p_jmp_buf = png_set_longjmp_fn( png_ptr, longjmp, sizeof( jmp_buf ) );

	if ( setjmp( *p_jmp_buf ) != -1 )
	{
		// Setup the writer
		png_set_write_fn( png_ptr, fp, png_write_data_fn, png_flush_data_fn );

		// Setup the header
		png_set_IHDR( png_ptr, info_ptr, uint32_t( image._width ), uint32_t( image._height ),
					  8 /*CHANNEL DEPTH*/, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
					  PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE );

		// Write!
		png_write_info( png_ptr, info_ptr );

		for ( int i = 0; i < image._height; ++i )
		{
			png_bytep row = (png_bytep)( image._data_ptr + ( i * image._width ) );

			png_write_rows( png_ptr, &row, 1 );
		}

		png_write_end( png_ptr, nullptr );

		std::cout << "OK\n";
	}

	// Destroy the main writer and info structures
	png_destroy_write_struct( &png_ptr, &info_ptr );

	fclose( fp );
}

void write_png_idx( const indexmap_t& image, std::vector< color_t >& aPalette, const std::string& strOutFile )
{
	// Open
	std::cout << "Writing \"" << strOutFile << "\" (" << image._uBPP << "-BPP) ... ";

	FILE* fp = nullptr;
	int e = fopen_s( &fp, strOutFile.c_str(), "wb" );
	if ( e != 0 || fp == nullptr )
	{
		std::cout << "ERROR (attempted overwrite?)\n\n";
		return;
	}

	// Initialise the PNG.
	png_structp png_ptr;
	png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, nullptr, png_error_fn, png_warn_fn );
	if ( png_ptr == nullptr )
	{
		std::cout << "ERROR: png_create_write_struct failed.\n";
		return;
	}

	// Initialise the information structure.
	png_infop info_ptr;
	info_ptr = png_create_info_struct( png_ptr );
	if ( info_ptr == nullptr )
	{
		std::cout << "ERROR: png_create_info_struct failed.\n";
		return;
	}

	// Palette!
	png_color palette[ 256 ];

	// dummy grey-scale palette.
	for ( uint32_t i = 0; i < 256; ++i )
	{
		palette[ i ].red = palette[ i ].green = palette[ i ].blue = static_cast<png_byte>( 256 - i );
	}

	for ( size_t i = 0; i < aPalette.size(); ++i )
	{
		const color_t src = aPalette[ i ];
		png_color* p = palette + i;
		p->red = src.chan[ 0 ];
		p->green = src.chan[ 1 ];
		p->blue = src.chan[ 2 ];
	}

	jmp_buf* p_jmp_buf = png_set_longjmp_fn( png_ptr, longjmp, sizeof( jmp_buf ) );

	if ( setjmp( *p_jmp_buf ) != -1 )
	{
		// Setup the writer
		png_set_write_fn( png_ptr, fp, png_write_data_fn, png_flush_data_fn );

		// Setup the header
		png_set_IHDR( png_ptr, info_ptr, image._width, image._height,
					  image._uBPP /*CHANNEL DEPTH*/, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
					  PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE );

		// palette
		png_set_PLTE( png_ptr, info_ptr, palette, ( 1 << image._uBPP ) );

		// Write!
		png_write_info( png_ptr, info_ptr );

		for ( int i = 0; i < image._height; ++i )
		{
			png_bytep row = const_cast<png_bytep>( image._data_ptr + ( i * image._uStride ) );

			png_write_rows( png_ptr, &row, 1 );
		}

		png_write_end( png_ptr, nullptr );

		std::cout << "OK\n";
	}

	// Destroy the main writer and info structures
	png_destroy_write_struct( &png_ptr, &info_ptr );

	fclose( fp );
}

//==============================================================================

static void accumulate_error( int x, int y, dithermap_t& workspace, const dither_t& error, float fScale )
{
	// out of bounds?
	if ( x < 0 || y < 0 || x >= workspace._width || y >= workspace._height )
		return;

	dither_t& p = workspace.Element( x, y );
	p.err_r += error.err_r * fScale;
	p.err_g += error.err_g * fScale;
	p.err_b += error.err_b * fScale;
}

static void remap_image_dither( const colormap_t& image, indexmap_t& output, options_t& options, const color_t pal_idx0 )
{
	dithermap_t workspace;
	workspace.Create( image._width, image._height );

	// load the workspace with the source image.
	for ( uint32_t y = 0; y < image._height; ++y )
	{
		for ( uint32_t x = 0; x < image._width; ++x )
		{
			color_t colour = image.Peek( x, y );

			dither_t& target = workspace.Element( x, y );
			target.err_r = static_cast<float>( colour.chan[ 0 ] ) / 255.0f;
			target.err_g = static_cast<float>( colour.chan[ 1 ] ) / 255.0f;
			target.err_b = static_cast<float>( colour.chan[ 2 ] ) / 255.0f;
			target.index = 0;
		}
	}

	// Floyd–Steinberg dithering
	for ( uint32_t y = 0; y < image._height; ++y )
	{
		for ( uint32_t x = 0; x < image._width; ++x )
		{
			dither_t& pixel = workspace.Element( x, y );

			// decide which is our closest palette index
			color_t old_colour_sat;
			old_colour_sat.FromDither( pixel );
			uint8_t remapped_idx = find_nearest_palette_index( old_colour_sat, options.aPalette );
			pixel.index = remapped_idx; // store this.

			// not an exact match? (likely)
			const color_t new_colour_sat = options.aPalette[ remapped_idx ];
			if ( old_colour_sat.BGR() != new_colour_sat.BGR() )
			{
				// compute error between the pixel and the palette value we must use.
				dither_t quant_error;
				quant_error.err_r = ( static_cast<float>( old_colour_sat.chan[ 0 ] ) - static_cast<float>( new_colour_sat.chan[ 0 ] ) ) / 255.0f;
				quant_error.err_g = ( static_cast<float>( old_colour_sat.chan[ 1 ] ) - static_cast<float>( new_colour_sat.chan[ 1 ] ) ) / 255.0f;
				quant_error.err_b = ( static_cast<float>( old_colour_sat.chan[ 2 ] ) - static_cast<float>( new_colour_sat.chan[ 2 ] ) ) / 255.0f;

				// diffuse the error among the neighbours
				accumulate_error( x + 1, y, workspace, quant_error, 7.0f / 16.0f );
				accumulate_error( x - 1, y + 1, workspace, quant_error, 3.0f / 16.0f );
				accumulate_error( x, y + 1, workspace, quant_error, 5.0f / 16.0f );
				accumulate_error( x + 1, y + 1, workspace, quant_error, 1.0f / 16.0f );
			}
		}
	}

	// copy the pixel indices into the output image
	for ( uint32_t y = 0; y < image._height; ++y )
	{
		for ( uint32_t x = 0; x < image._width; ++x )
		{
			const dither_t& pixel = workspace.Element( x, y );
			output.Plot( x, y, pixel.index );
		}
	}

	delete[] workspace._data_ptr;
}

//==============================================================================

static void remap_image_nearest( const colormap_t& image, indexmap_t& output, options_t& options, const color_t pal_idx0 )
{
	std::vector< color_t >& aPalette = options.aPalette;

	for ( uint32_t y = 0; y < image._height; ++y )
	{
		for ( uint32_t x = 0; x < image._width; ++x )
		{
			const color_t colour = image.Peek( x, y );

			uint8_t remapped_idx;

			remapped_idx = find_nearest_palette_index( colour, aPalette );

			output.Plot( x, y, remapped_idx );
		}
	}
}

//==============================================================================

static void resize_image_nearest( colormap_t& output, const colormap_t& input )
{
	std::cout << "Resizing to (" << output._width << " x " << output._height << ") - 'nearest neighbor'\n";

	for ( int ry = 0; ry < output._height; ++ry )
	{
		for ( int rx = 0; rx < output._width; ++rx )
		{
			float x, y;
			x = float( rx * input._width ) / float( output._width );
			y = float( ry * input._height ) / float( output._height );

			const color_t colour = input.Peek( int( floorf( x ) ), int( floorf( y ) ) );

			output.Plot( rx, ry, colour );
		}
	}
}

static void resize_image_bilinear( colormap_t& output, const colormap_t& input )
{
	std::cout << "Resizing to (" << output._width << " x " << output._height << ") - 'bilinear'\n";

	float x, y, fx, fy;
	int ix, iy;

	for ( int ry = 0; ry < output._height; ++ry )
	{
		y = ( float( ( ry + 0.5f ) * input._height ) / float( output._height ) ) - 0.5f;
		iy = int( floorf( y ) );
		fy = y - iy;

		for ( int rx = 0; rx < output._width; ++rx )
		{
			x = ( float( ( rx + 0.5f ) * input._width ) / float( output._width ) ) - 0.5f;
			ix = int( floorf( x ) );
			fx = x - ix;

			// ... 4 taps
			const color_t colour00 = input.PeekClamp( ix, iy );
			const color_t colour10 = input.PeekClamp( ix + 1, iy );
			const color_t colour01 = input.PeekClamp( ix, iy + 1 );
			const color_t colour11 = input.PeekClamp( ix + 1, iy + 1 );

			// ... bilinear interpolation (float)
			fcolor_t colour_a = fcolor_t::Blend( colour00, colour10, fx );
			fcolor_t colour_b = fcolor_t::Blend( colour01, colour11, fx );
			fcolor_t colour = fcolor_t::Blend( colour_a, colour_b, fy );

			// ... quantise to 8-bit channels
			color_t out;
			for ( int i = 0; i < 4; ++i )
			{
				out.chan[ i ] = uint8_t( std::clamp( int( floorf( colour.chan[ i ] + 0.5f ) ), 0, 255 ) );
			}

 			output.Plot( rx, ry, out );
		}
	}
}

//==============================================================================

//
// determine_output_filename
//
// Figure out what we should call the output filename
//
static void determine_output_filename( const std::string& inputFile, options_t& options, std::string& outFile )
{
	// Use the single file output override?
	if ( ( options.aInputFiles.size() == 1 ) && ( options.strOutFile.empty() == false ) )
	{
		outFile = options.strOutFile;
		return;
	}

	// split input file into path and file.
	std::string outFolder;
	size_t slash_find = inputFile.find_last_of( "/\\", inputFile.npos ); // either kind of path separators
	if ( slash_find == inputFile.npos )
	{
		outFile = inputFile;
	}
	else
	{
		outFolder = inputFile.substr( 0, slash_find + 1 );
		outFile = inputFile.substr( slash_find + 1 );
	}

	if ( options.strOutFolder.empty() == false )
	{
		outFolder = options.strOutFolder;
	}

	size_t dot_find = outFile.find_last_of( ".", outFile.npos ); // last dot
	if ( dot_find != inputFile.npos )
	{
		outFile = outFile.substr( 0, dot_find );
	}

	if ( outFolder.empty() == false )
	{
		const char tail = outFolder[ outFolder.length() - 1 ];

		if ( tail != '\\' && tail != '//' )
			outFolder += "\\";
	}

	outFile = outFolder + outFile + ".png";
}

//
// do_work
//
// Palette generator.
//
static void do_work( options_t& options )
{
	if ( options.aPalette.empty() == false )
	{
		std::cout << "Applying palette \"" << options.strPaletteFile << "\". It has " << options.aPalette.size() << " entries.\n";
	}

	if ( options.aInputFiles.size() > 1 )
	{
		std::cout << "Processing " << options.aInputFiles.size() << " files...\n";
	}

	// ... auto-create the output folder, if specified.
	if ( options.strOutFolder.empty() == false )
	{
		make_path( options.strOutFolder );
	}

	uint8_t uBPP;

	if ( options.aPalette.empty() )
	{
		uBPP = 24;
	}
	else if ( options.aPalette.size() == 2 )
	{
		uBPP = 1;
	}
	else if ( options.aPalette.size() <= 4 )
	{
		uBPP = 2;
	}
	else if ( options.aPalette.size() <= 16 )
	{
		uBPP = 4;
	}
	else
	{
		uBPP = 8;
	}

	std::vector< color_t > png_palette;

	if ( uBPP <= 8 )
	{
		for ( uint32_t i = 0; i < options.aPalette.size(); ++i )
		{
			png_palette.push_back( options.aPalette[ i ] );
		}
		while ( png_palette.size() < ( 1ULL << uBPP ) )
		{
			png_palette.push_back( color_t( 0xFF00FF ) );
		}
	}

	//
	// -- Process Each File

	for ( const std::string& inputFile : options.aInputFiles )
	{
		// load the image.

		int w, h, chan_count;
		unsigned char* img_data;

		std::cout << "Loading \"" << inputFile << "\" ... ";

		img_data = stbi_load( inputFile.c_str(), &w, &h, &chan_count, 0 );

		if ( img_data == nullptr )
		{
			std::cout << "FAILED\n";
			continue;
		}
		else if ( chan_count != 3 && chan_count != 4 )
		{
			std::cout << "INVALID-CHANNELS (" << chan_count << ")\n";
			stbi_image_free( img_data );
			continue;
		}

		// Re-open input file - keep it open to prevent common user error of overwriting input!
		std::ifstream fileInput( inputFile );
		if ( fileInput.is_open() == false )
		{
			std::cout << "FAILED\n";
			stbi_image_free( img_data );
			continue;
		}

		std::cout << "OK (" << w << " x " << h << ")\n";

		// where do we write the output?
		std::string outFile;
		determine_output_filename( inputFile, options, outFile );

		colormap_t original;
		original.Create( w, h );
		if ( chan_count == 3 )
		{
			original.CopyFromRGB( img_data );
		}
		else if ( chan_count == 4 )
		{
			original.CopyFromRGBA( img_data );
		}

		size_t out_width, out_height;

		if ( options.width == 0 )
		{
			out_width = w * options.height / h;
		}
		else
		{
			out_width = options.width;
		}

		if ( options.height == 0 )
		{
			out_height = h * options.width / w;
		}
		else
		{
			out_height = options.height;
		}

		colormap_t resize;
		resize.Create( out_width, out_height );

		switch ( options.filter )
		{

		default:
		case FILTER_NEAREST:
			resize_image_nearest( resize, original );
			break;

		case FILTER_BILINEAR:
			{
				// ... handle large reductions using a pyramid of step-downs
				for ( ; ; )
				{
					size_t x = original._width;
					size_t y = original._height;

					bool inter_step = false;

					if ( ( out_width * 2 < x ) )
					{
						x /= 2;
						inter_step = true;
					}

					if ( ( out_height * 2 < y ) )
					{
						y /= 2;
						inter_step = true;
					}

					if ( inter_step )
					{
						colormap_t pyramid;
						pyramid.Create( x, y );
						resize_image_bilinear( pyramid, original );
						delete[] original._data_ptr;
						original = pyramid;
					}
					else
					{
						break;
					}
				}

				resize.Create( out_width, out_height );
				resize_image_bilinear( resize, original );
			}
			break;

		}

		if ( png_palette.empty() )
		{
			// write image!
			write_png_rgb24( resize, outFile );
		}
		else
		{
			indexmap_t output;
			output.Create( int( out_width ), int( out_height ), uBPP );

			const color_t pal_idx0 = options.aPalette[ 0 ];

			if ( options.bDither )
			{
				remap_image_dither( resize, output, options, pal_idx0 );
			}
			else
			{
				remap_image_nearest( resize, output, options, pal_idx0 );
			}

			// write image!
			write_png_idx( output, options.aPalette, outFile );

			delete[] output._data_ptr;
		}

		// tidy up
		delete[] original._data_ptr;
		delete[] resize._data_ptr;
		stbi_image_free( img_data );
		fileInput.close();
	}
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
		print_hello();

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

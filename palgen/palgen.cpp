
/*

MIT License

Copyright (c) 2024 David Walters

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
#include <cstdio>
#include <set>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>

#include <io.h>

#define STBI_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define KEY_TRANSPARENT		0x00ff00ff

//=============================================================================

struct color_t
{
	union 
	{
		uint32_t value_abgr;
		uint8_t chan[ 4 ]; // R, G, B, A
	};

public:
	inline uint32_t sum_rgb()
	{
		return (int)chan[ 0 ] + (int)chan[ 1 ] + (int)chan[ 2 ];
	}
};

struct sColorTotal
{
	color_t _colAverage;

	size_t _uScaledRGBA[ 4 ]; // R, G, B, A
	size_t _uTotal;

public:

	sColorTotal( uint32_t key, size_t total )
	{
		Set( key, total );
	}

	void Set( uint32_t key, size_t total )
	{
		_colAverage.value_abgr = key;

		_uScaledRGBA[ 0 ] = _colAverage.chan[ 0 ] * total;
		_uScaledRGBA[ 1 ] = _colAverage.chan[ 1 ] * total;
		_uScaledRGBA[ 2 ] = _colAverage.chan[ 2 ] * total;
		_uScaledRGBA[ 3 ] = _colAverage.chan[ 3 ] * total;

		_uTotal = total;
	}

	void GenerateAverage()
	{
		for ( int i = 0; i < 4; ++i )
		{
			_colAverage.chan[ i ] = static_cast<uint8_t>( std::clamp< size_t >( _uScaledRGBA[ i ] / _uTotal, 0, 255 ) );
		}
	}
};

struct options_t
{
	std::set< std::string > aInputFiles;
	
	std::string strOutFile;

	uint32_t uPaletteSizeReal = 256;
	uint32_t uPaletteSizePow2 = 256;

	bool bForceTransp = false;
	bool bForceOpaque = false;
};

typedef std::unordered_map< uint32_t, size_t > tUniqueColorMap;

typedef std::vector< sColorTotal* > tColorBucket;

//=============================================================================

static uint32_t blend_rgb( color_t a, color_t b )
{
	color_t out;

	int mix;

	out.chan[ 3 ] = 0xFF;

	for ( int i = 0; i < 3; ++i )
	{
		mix = ( ( (int)a.chan[ i ] ) + ( (int)b.chan[ i ] ) ) >> 1;
		out.chan[ i ] = static_cast<uint8_t>( mix );
	}

	return out.value_abgr;
}

//
// rgb_color_distance_squared
//
// Compute a 'distance' between too colors. Smaller values are closer.
//
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

static void crush_palette( std::vector< color_t >& aPalette, size_t targetSize )
{
	while ( aPalette.size() > targetSize )
	{
		struct measure_t
		{
			size_t index0;
			size_t index1;
			int best_dist;
		};

		measure_t m{ UINT32_MAX, UINT32_MAX, INT32_MAX };

		for ( size_t i = 0; i < aPalette.size(); ++i )
		{
			const color_t& col_i = aPalette[ i ];

			for ( size_t j = 0; j < aPalette.size(); ++j )
			{
				if ( i == j )
					continue;

				const color_t& col_j = aPalette[ j ];

				int dist = rgb_color_distance_squared( col_i, col_j );
				if ( dist <= m.best_dist )
				{
					m.index0 = i;
					m.index1 = j;
					m.best_dist = dist;
				}
			}
		}

		std::vector< color_t > aNewPal;
		
		for ( size_t i = 0; i < aPalette.size(); ++i )
		{
			if ( i == m.index0 || i == m.index1 )
				continue;

			aNewPal.push_back( aPalette[ i ] );
		}

		aNewPal.push_back( color_t( blend_rgb( aPalette[ m.index0 ], aPalette[ m.index1 ] ) ) );

		std::swap( aPalette, aNewPal );
	}
}

//
// next_power_two
//
// Returns the closest power-of-two number greater or equal
// to n for the given (unsigned) integer n.
// Will return 0 when n = 0, 1 when n = 1.
//
static constexpr uint32_t next_power_two( uint32_t n )
{
	--n;
	n |= n >> 16;
	n |= n >> 8;
	n |= n >> 4;
	n |= n >> 2;
	n |= n >> 1;
	++n;
	return n;
}

//
// print_hello
//
// Print hello text
//
static void print_hello()
{
	printf( "\n------------------------------------------------------------------\n"
			" Palette Generator (c) David Walters. See LICENSE.txt for details\n"
			"------------------------------------------------------------------\n\n" );
}

//
// print_help
//
// Print help text
//
static void print_help()
{
	// Usage
	printf( " USAGE: palgen.exe [-?] [-count=#] [-transp] [-opaque] <image>[...] -o <palette>\n" );
	putchar( '\n' );

	// Options
	printf( "  -?                This help.\n" );
	printf( "  -count=#          Set the palette size. [Default=256]\n" );
	printf( "  -transp           Always make index 0 transparent.\n" );
	printf( "  -opaque           Ignore transparent pixels.\n" );
	putchar( '\n' );
	printf( "  <image>           Source image(s), wildcards supported.\n" );
	putchar( '\n' );
	printf( "  -o <palette>      Filename of output palette.\n" );
	putchar( '\n' );

	putchar( '\n' );
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
	bool bNextArgIsOutput = false;

	// Parse Command Line
	for ( int iarg = 1; iarg < argc; ++iarg ) // skip element[0]
	{
		const char* szArg = argv[ iarg ];

		if ( bNextArgIsOutput )
		{
			bNextArgIsOutput = false;
			options.strOutFile = szArg;
		}
		else if ( strncmp( szArg, "-count=", 7 ) == 0 )
		{
			int iSize = atoi( szArg + 7 );

			if ( iSize <= 2 )
			{
				printf( "Error - invalid palette size (%d).\n", iSize );
				return false;
			}

			options.uPaletteSizeReal = iSize;
			options.uPaletteSizePow2 = next_power_two( iSize );
		}
		else if ( _stricmp( szArg, "-?" ) == 0 )
		{
			return false;
		}
		else if ( _stricmp( szArg, "-o" ) == 0 )
		{
			bNextArgIsOutput = true;
		}
		else if ( _stricmp( szArg, "-transp" ) == 0 )
		{
			options.bForceTransp = true;
		}
		else if ( _stricmp( szArg, "-opaque" ) == 0 )
		{
			options.bForceOpaque = true;
		}
		else
		{
			add_files_wildcard( szArg, options.aInputFiles );
		}

	}; // for each command line argument

	if ( options.aInputFiles.empty() )
	{
		printf( "Error - no input file(s) specified.\n" );
		return false;
	}

	if ( options.strOutFile.empty() )
	{
		printf( "Error - no output file specified.\n" );
		return false;
	}

	return true;
}

//
// write_hexfile
//
// Dump the whole palette to disk in .hex format.
//
static void write_hexfile( std::vector< color_t >& aPalette, const std::string& strFileName )
{
	char buf[ 64 ];

	printf( "Writing \"%s\" ... ", strFileName.c_str() );

	std::ofstream file( strFileName );
	if ( file.is_open() )
	{
		for ( uint32_t i = 0; i < aPalette.size(); ++i )
		{
			// read BGRA palette entry and convert to RGB

			const color_t& pal = aPalette[ i ];
			uint32_t cout;
			cout = pal.chan[ 0 ] << 16;
			cout |= pal.chan[ 1 ] << 8;
			cout |= pal.chan[ 2 ];

			// .hex palette file format
			sprintf_s( buf, sizeof( buf ), "%06x\n", cout );

			file << buf;
		}

		file.close();
		printf( "OK\n\n" );
	}
	else
	{
		printf( "FAILED\n\n" );
	}
}


static void count_unique_image_cols_3ch( uint8_t* data, int width, int height, tUniqueColorMap& col_counts )
{
	for ( int y = 0; y < height; ++y )
	{
		for ( int x = 0; x < width; ++x )
		{
			color_t col;
			col.chan[ 0 ] = data[ 0 ];
			col.chan[ 1 ] = data[ 1 ];
			col.chan[ 2 ] = data[ 2 ];
			col.chan[ 3 ] = 0xFF;

			data += 3;

			// Add to the map
			col_counts[ col.value_abgr ]++;
		}
	}
}

static void count_unique_image_cols_4ch( uint8_t* data, int width, int height, tUniqueColorMap& col_counts, bool& bMaskDetected )
{
	for ( int y = 0; y < height; ++y )
	{
		for ( int x = 0; x < width; ++x )
		{
			color_t col;
			col.chan[ 0 ] = data[ 0 ];
			col.chan[ 1 ] = data[ 1 ];
			col.chan[ 2 ] = data[ 2 ];
			col.chan[ 3 ] = data[ 3 ];

			data += 4;

			// not-opaque pixel?
			if ( ( col.chan[ 3 ] ) != 0xFF )
			{
				bMaskDetected = true;
				continue;
			}

			// Add to the map
			col_counts[ col.value_abgr ]++;
		}
	}
}

//
// analyse_images
//
// Load and run count_unique_image_cols on all image files to build a unique color map.
//
static void analyse_images( const std::set< std::string >& file_names,
							tUniqueColorMap& unique_colors, 
							bool& bMaskDetected )
{
	for ( const auto& file_name : file_names )
	{
		int w, h, chan_count;
		unsigned char* data;
		
		std::cout << "Analyze: \"" << file_name << "\" ... ";

		data = stbi_load( file_name.c_str(), &w, &h, &chan_count, 0 );

		if ( data == nullptr )
		{
			std::cout << "FAILED\n";
			continue;
		}
		else if ( chan_count != 3 && chan_count != 4 )
		{
			std::cout << "INVALID-CHANNELS (" << chan_count << ")\n";
			continue;
		}
		else
		{
			std::cout << "LOADED (" << w << " x " << h << ") ... ";

			switch ( chan_count )
			{

			case 3:
				count_unique_image_cols_3ch( data, w, h, unique_colors );
				break;

			case 4:
				count_unique_image_cols_4ch( data, w, h, unique_colors, bMaskDetected );
				break;

			}

			stbi_image_free( data );

			std::cout << "OK\n";
		}
	}
}

//==============================================================================

static int median_find_axis( const tColorBucket& aBucket )
{
	int big_axis = 1;

	int r_min = 0xFF, g_min = 0xFF, b_min = 0xFF;
	int r_max = 0, g_max = 0, b_max = 0;

	for ( const sColorTotal* pTotal : aBucket )
	{
		const color_t& color = pTotal->_colAverage;

		if ( color.chan[ 3 ] != 0xFF )
			continue;

		r_min = std::min< int >( r_min, color.chan[ 0 ] );
		g_min = std::min< int >( g_min, color.chan[ 1 ] );
		b_min = std::min< int >( b_min, color.chan[ 2 ] );

		r_max = std::max< int >( r_max, color.chan[ 0 ] );
		g_max = std::max< int >( g_max, color.chan[ 1 ] );
		b_max = std::max< int >( b_max, color.chan[ 2 ] );
	}

	int r_range, g_range, b_range;

	r_range = r_max - r_min;
	g_range = g_max - g_min;
	b_range = b_max - b_min;

	if ( g_range >= r_range && g_range >= b_range )
		big_axis = 1; // G
	else if ( b_range >= r_range && b_range >= g_range )
		big_axis = 2; // B
	else if ( r_range >= g_range && r_range >= b_range )
		big_axis = 0; // R

	return big_axis;
}

static void median_sort_bucket( tColorBucket& aBucket, int channel_axis )
{
	if ( aBucket.size() < 2 )
	{
		return;
	}

	uint32_t mask;
	mask = 0xFF << ( channel_axis * 8 );

	// sort along the given channel axis
	std::sort( aBucket.begin(), aBucket.end(), [&]( sColorTotal* t1, sColorTotal* t2 )
			   {
				   const uint32_t col1 = t1->_colAverage.value_abgr;
				   const uint32_t col2 = t2->_colAverage.value_abgr;

				   // move larger values to the end of the array.
				   return ( col1 & mask ) < ( col2 & mask );
			   } );
}

static uint32_t find_median_bucket_final_color( const tColorBucket& aBucket )
{
	if ( aBucket.empty() )
	{
		return 0;
	}

	sColorTotal mega( 0, 0 );

	for ( const sColorTotal* pColor : aBucket )
	{
		mega._uTotal += pColor->_uTotal;

		mega._uScaledRGBA[ 0 ] += pColor->_uScaledRGBA[ 0 ];
		mega._uScaledRGBA[ 1 ] += pColor->_uScaledRGBA[ 1 ];
		mega._uScaledRGBA[ 2 ] += pColor->_uScaledRGBA[ 2 ];
		mega._uScaledRGBA[ 3 ] += pColor->_uScaledRGBA[ 3 ];
	}

	mega.GenerateAverage();

	return mega._colAverage.value_abgr;
}

static void median_cut_inner( tColorBucket& aSourceBucket, std::vector< tColorBucket* >& aOutBuckets )
{
	int axis = median_find_axis( aSourceBucket );

	median_sort_bucket( aSourceBucket, axis );

	const size_t median_index = ( ( aSourceBucket.size() + 1 ) / 2 );

	tColorBucket* bucket_a, * bucket_b;

	bucket_a = new tColorBucket;
	for ( size_t i = 0; i < median_index; ++i )
	{
		bucket_a->push_back( aSourceBucket[ i ] );
	}

	bucket_b = new tColorBucket;
	for ( size_t i = median_index; i < aSourceBucket.size(); ++i )
	{
		bucket_b->push_back( aSourceBucket[ i ] );
	}

	aOutBuckets.push_back( bucket_a );
	aOutBuckets.push_back( bucket_b );
}

void median_cut( tUniqueColorMap& unique_colors, const uint32_t max_colors, std::vector< color_t >& aPalette )
{
	// transfer the unique color map into a vector
	tColorBucket* pOriginalBucket = new tColorBucket();
	for ( const auto& [key, value] : unique_colors )
	{
		pOriginalBucket->push_back( new sColorTotal( key, value ) );
	}

	// initial split of full color list.
	std::vector< tColorBucket* > aBuckets;
	median_cut_inner( *pOriginalBucket, aBuckets );

	// can we subdivide further?
	while ( aBuckets.size() * 2 <= max_colors )
	{
		std::vector< tColorBucket* > aNewBuckets;

		for ( tColorBucket* pBucket : aBuckets )
		{
			median_cut_inner( *pBucket, aNewBuckets );
			delete pBucket;
		}

		aBuckets.clear();
		std::swap( aBuckets, aNewBuckets );
	}

	// transfer color averages into the final palette.
	for ( const tColorBucket* pBucket : aBuckets )
	{
		color_t color;
		color.value_abgr = find_median_bucket_final_color( *pBucket );

		aPalette.push_back( color );

		// clean up the bucket
		for ( sColorTotal* pTotal : *pBucket )
		{
			delete pTotal;
		}

		delete pBucket;
	}

	delete pOriginalBucket;
}

//==============================================================================

static void sort_palette_rgb( std::vector< color_t >& aPalette )
{
	if ( aPalette.size() < 2 )
		return;

	std::sort( aPalette.begin(), aPalette.end(), [&]( color_t t1, color_t t2 )
			   {
				   // move larger values to the end of the array.
				   return ( t1.sum_rgb() ) < ( t2.sum_rgb() );
			   } );
}

//==============================================================================

//
// do_work
//
// Palette generator.
//
static void do_work( const options_t& options )
{
	std::vector< color_t > aPalette;
	bool bMaskDetected = false;

	print_hello();

	if ( options.aInputFiles.size() > 1 )
	{
		std::cout << "Analyzing " << options.aInputFiles.size() << " files ...\n";
	}

	tUniqueColorMap unique_colors;

	analyse_images( options.aInputFiles, unique_colors, bMaskDetected );

	std::cout << "\nDetected " << unique_colors.size() << " unique colors.\n";

	{
		std::cout << "Applying 'median cut' reduction... ";

		median_cut( unique_colors, options.uPaletteSizePow2, aPalette );

		uint32_t targetSize = options.uPaletteSizeReal;
		if ( ( bMaskDetected && !options.bForceOpaque ) || options.bForceTransp )
		{
			--targetSize;
		}

		crush_palette( aPalette, targetSize );

		sort_palette_rgb( aPalette ); // move black to index 0
	}

	if ( aPalette.empty() )
	{
		std::cout << "\nError - no palette was generated.\n";
		return;
	}

	std::cout << "DONE.\n";

	if ( ( bMaskDetected && !options.bForceOpaque ) || options.bForceTransp )
	{
		std::cout << "Reduced palette to " << aPalette.size() << ". Plus transparent index 0.\n\n";

		aPalette.insert( aPalette.begin(), color_t( KEY_TRANSPARENT ) );
	}
	else
	{
		std::cout << "Reduced palette to " << aPalette.size() << ".\n\n";
	}

	// Try and write the output.
	write_hexfile( aPalette, options.strOutFile );
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

/*
	General enhancements and units support by David Walters, 2024.
	Released under the OLC-3 license. See below.

	Derived from:

	DIY Programming Language #2
	Parsing Text To Tokens with a Finite State Machine

	by David Barr, aka javidx9, ©OneLoneCoder 2024

	Original Source: https://github.com/OneLoneCoder/Javidx9/blob/master/SimplyCode/OneLoneCoder_DIYLanguage_Tokenizer.cpp

	Relevant Video: https://youtu.be/wrj3iuRdA-M

	NOTE: The use case was changed from a general purpose programming language
	expression parser. This code is intended to meet the requirements of an
	advanced numeric edit field for some hypothetical design application, where
	units and simple arithmetic would be convenient to enter sizes or distances.

	The namespace renamed from olc::lang to Numeric (as in numeric expression)

	License (OLC-3)
	~~~~~~~~~~~~~~~

	Copyright 2018-2024 OneLoneCoder.com

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:

	1. Redistributions or derivations of source code must retain the above
	copyright notice, this list of conditions and the following disclaimer.

	2. Redistributions or derivative works in binary form must reproduce
	the above copyright notice. This list of conditions and the following
	disclaimer must be reproduced in the documentation and/or other
	materials provided with the distribution.

	3. Neither the name of the copyright holder nor the names of its
	contributors may be used to endorse or promote products derived
	from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
	A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
	HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
	SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
	LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <string>
#include <iostream>
#include <locale>

#include "numeric.h"

//==============================================================================

#define OUTPUT_TO_CM						0
#define OUTPUT_TO_YARDS						0

#define DEBUG_OUTPUT_TOKENS					0
#define DEBUG_OUTPUT_RPN					0
#define DEBUG_OUTPUT_RPN_EXTRA				0
#define DEBUG_OUTPUT_ERROR					0

static constexpr auto gWhitespaceDigits = Numeric::lut::MakeLUT( " \t\n\r\v\f" );
static constexpr auto gFirstNumericDigits = Numeric::lut::MakeLUT( "0123456789" );
static constexpr auto gAdditionalNumericDigits = Numeric::lut::MakeLUT( ".,0123456789" );
static constexpr auto gOperatorDigits = Numeric::lut::MakeLUT( "*+-/" );
static constexpr auto gUnitDigits = Numeric::lut::MakeLUT( "mMkKcfootfeetinchesyardsmiles'\"" );
static constexpr auto gAllowedHexDigits = Numeric::lut::MakeLUT( "0123456789abcdefABCDEF" );
static constexpr auto gAllowedBinaryDigits = Numeric::lut::MakeLUT( "01" );

static constexpr int gDenominatorTable[] = { 2, 3, 4, 5, 6, 7, 8, 10, 12, 16, 32, 64, 128, 1000, -1 };

//==============================================================================

std::string Numeric::Token::str() const
{
	std::string o;

	switch ( type )
	{
	case Token::Type::Unknown:					o += "[Unknown           ]"; break;
	case Token::Type::Literal_Numeric:			o += "[Literal, Numeric  ]"; break;
	case Token::Type::Operator:					o += "[Operator          ]"; break;
	case Token::Type::Parenthesis_Open:			o += "[Parenthesis, Open ]"; break;
	case Token::Type::Parenthesis_Close:		o += "[Parenthesis, Close]"; break;
	case Token::Type::Symbol:					o += "[Symbol            ]"; break;
	case Token::Type::Unit:						o += "[Unit              ]"; break;
	}

	o += " @ (" + std::to_string( pos ) + ") : " + text;

	if ( type == Type::Literal_Numeric )
	{
		o += " [" + std::to_string( value ) + "]";
	}
	else if ( type == Type::Unit )
	{
		o += " [x" + std::to_string( value ) + "]";
	}

	return o;
}

Numeric::CompilerError::CompilerError( Stage stage, const std::string& sMsg )
{
	switch ( stage )
	{

	case Stage::Parser:
		_message = "[PARSE] " + sMsg;
		break;

	case Stage::Solver:
		_message = "[SOLVE] " + sMsg;
		break;

	}
}

Numeric::Compiler::Compiler()
{
	// Standard Binary Operators
	_mapOperators[ "*" ] = { 3, 2 };
	_mapOperators[ "/" ] = { 3, 2 };
	_mapOperators[ "+" ] = { 1, 2 };
	_mapOperators[ "-" ] = { 1, 2 };

	// Unary Operators
	_mapOperators[ "u+" ] = { 100, 1 };
	_mapOperators[ "u-" ] = { 100, 1 };

	// Default
	SetUnitOut( UnitType::Generic );
}

void Numeric::Compiler::SetUnitOut( const UnitType type )
{
	_mapUnits.clear();
	_mapUnitLookup.clear();

	_desiredUnitType = type;

	if ( type == UnitType::Imperial )
	{
		//
		// -- IMPERIAL SYSTEM

		// Metric Units
		_mapUnits[ "mm" ] = { 1.0 / _metricScaleInch, UnitType::Metric };
		_mapUnits[ "cm" ] = { 10.0 / _metricScaleInch, UnitType::Metric };
		_mapUnits[ "m" ] = { 1000.0 / _metricScaleInch, UnitType::Metric };
		_mapUnits[ "Km" ] = _mapUnits[ "km" ] = { 1000000.0 / _metricScaleInch, UnitType::Metric };
		_mapUnits[ "Mm" ] = { 1000000000.0 / _metricScaleInch, UnitType::Metric };

		// Metric Units (lookup)
		_mapUnitLookup[ 0.001 / _metricScaleFoot ] = "mm";
		_mapUnitLookup[ 0.01 / _metricScaleFoot ] = "cm";
		_mapUnitLookup[ 1 / _metricScaleFoot ] = "m";
		_mapUnitLookup[ 1000 / _metricScaleFoot ] = "Km";
		_mapUnitLookup[ 1000000 / _metricScaleFoot ] = "Mm";

		// Imperial Units
		_mapUnits[ "th" ] = _mapUnits[ "thou" ] = _mapUnits[ "mil" ] = { _impScaleThou, UnitType::Imperial };
		_mapUnits[ "in" ] = _mapUnits[ "inch" ] = _mapUnits[ "inches" ] = _mapUnits[ "\"" ] = { _impScaleInch, UnitType::Imperial };
		_mapUnits[ "ft" ] = _mapUnits[ "foot" ] = _mapUnits[ "feet" ] = _mapUnits[ "'" ] = { _impScaleFoot, UnitType::Imperial };
		_mapUnits[ "yd" ] = _mapUnits[ "yard" ] = _mapUnits[ "yds" ] = _mapUnits[ "yards" ] = { _impScaleYard, UnitType::Imperial };
		_mapUnits[ "mi" ] = _mapUnits[ "mile" ] = _mapUnits[ "miles" ] = { _impScaleMile, UnitType::Imperial };

		// Imperial Units (lookup)
		_mapUnitLookup[ _impScaleThou ] = "th";
		_mapUnitLookup[ _impScaleInch ] = "in";
		_mapUnitLookup[ _impScaleFoot ] = "ft";
		_mapUnitLookup[ _impScaleYard ] = "yd";
		_mapUnitLookup[ _impScaleMile ] = "mi";
	}
	else
	{
		//
		// -- METRIC SYSTEM

		// Metric Units
		_mapUnits[ "mm" ] = { 0.001, UnitType::Metric };
		_mapUnits[ "cm" ] = { 0.01, UnitType::Metric };
		_mapUnits[ "m" ] = { 1, UnitType::Metric };
		_mapUnits[ "Km" ] = _mapUnits[ "km" ] = { 1000, UnitType::Metric };
		_mapUnits[ "Mm" ] = { 1000000, UnitType::Metric };

		// Metric Units (lookup)
		_mapUnitLookup[ 0.001 ] = "mm";
		_mapUnitLookup[ 0.01 ] = "cm";
		_mapUnitLookup[ 1 ] = "m";
		_mapUnitLookup[ 1000 ] = "Km";
		_mapUnitLookup[ 1000000 ] = "Mm";

		// Imperial Units
		_mapUnits[ "in" ] = _mapUnits[ "inch" ] = _mapUnits[ "inches" ] = _mapUnits[ "\"" ] = { _metricScaleInch, UnitType::Imperial };
		_mapUnits[ "ft" ] = _mapUnits[ "foot" ] = _mapUnits[ "feet" ] = _mapUnits[ "'" ] = { _metricScaleFoot, UnitType::Imperial };
		_mapUnits[ "yd" ] = _mapUnits[ "yard" ] = _mapUnits[ "yds" ] = _mapUnits[ "yards" ] = { _metricScaleYard, UnitType::Imperial };
		_mapUnits[ "mi" ] = _mapUnits[ "mile" ] = _mapUnits[ "miles" ] = { _metricScaleMile, UnitType::Imperial };

		// Imperial Units (lookup)
		_mapUnitLookup[ _metricScaleInch ] = "in";
		_mapUnitLookup[ _metricScaleFoot ] = "ft";
		_mapUnitLookup[ _metricScaleYard ] = "yd";
		_mapUnitLookup[ _metricScaleMile ] = "mi";
	}
}

const Numeric::Unit Numeric::Compiler::DefaultUnit() const
{
	if ( _desiredUnitType == UnitType::Generic || _desiredUnitType == UnitType::Metric )
	{
		return { 1.0, _desiredUnitType };
	}
	else
	{
		return { _impScaleFoot, _desiredUnitType };
	}
}

Numeric::Solution Numeric::Compiler::Eval( const std::string& sInput, const Solution* pPrevSolution )
{
	// Parse the expression into tokens
	auto vTokens = Parse( sInput );

	// Solve the expression
	Solution result = Solve( vTokens, pPrevSolution );

	return result;
}

const std::string Numeric::Compiler::Format( const Solution& result ) const
{
	std::string out;

	double normalValue = result.value / result.units.scale;
	int64_t normalValueAbsInt = static_cast<int64_t>( floor( fabs( normalValue ) ) );
	double frac = fabs( normalValue ) - normalValueAbsInt;

	if ( IsEpsilonInteger( normalValue ) )
	{
		out = std::to_string( static_cast<int64_t>( round( normalValue ) ) );
	}
	else // frac > 0
	{
		bool success = false;

		if ( result.units.type == Numeric::UnitType::Imperial && _imperialFractions )
		{
			for ( const int* pDenominator = gDenominatorTable; *pDenominator != -1; ++pDenominator )
			{
				int denom = *pDenominator;

				if ( IsEpsilonInteger( frac * denom ) )
				{
					if ( normalValueAbsInt != 0 )
					{
						out += std::to_string( (int64_t)normalValue );

						if ( denom == 12 && result.units.scale == _impScaleFoot )
						{
							out += UnitName( result.units );
						}

						if ( normalValue < 0 )
						{
							out += "-";
						}
						else
						{
							out += "+";
						}
					}

					if ( denom == 12 && result.units.scale == _impScaleFoot )
					{
						out += std::to_string( int( frac * denom ) ) + UnitName( { _impScaleInch, UnitType::Imperial } );

						return out; // <== EARLY OUT
					}
					else
					{
						out += std::to_string( int( frac * denom ) ) + "/" + std::to_string( denom );
					}

					success = true;
					break;
				}
			}
		}

		if ( success == false )
		{
			out = std::to_string( normalValue );
		}
	}

	out += UnitName( result.units );

	return out;
}

std::vector<Numeric::Token> Numeric::Compiler::Parse( const std::string& sInput )
{
	if ( sInput.empty() )
	{
		throw CompilerError( CompilerError::Stage::Parser, "No input." );
	}

	// Cache the current decimal point used by the locale.
	_localeDecimalPoint = std::use_facet<std::numpunct<char>>( std::locale( "" ) ).decimal_point();

	std::vector< Token > vecOutputTokens;

	// Prepare input
	SetupInput( sInput );

	// Finite State Machine
	enum class TokeniserState
	{
		NewToken,
		NumericLiteral,
		PrefixedNumericLiteral,
		HexNumericLiteral,
		BinNumericLiteral,
		Unit_or_Symbol,
		Parenthesis_Open,
		Parenthesis_Close,
		Operator,
		CompleteToken,
	};

	// State
	TokeniserState stateNow = TokeniserState::NewToken;
	TokeniserState stateNext = TokeniserState::NewToken;
	std::string sCurrentToken = "";
	Token tokCurrent;
	Token tokPrevious = { _inputPos, Token::Type::Unknown, "" };
	size_t uTokenStart = 0;
	size_t uParenthesisBalance = 0;
	bool bDecimalPointFound = false;
	bool bFootFound = false;
	std::string sPrefixedNumericLiteralToken = "";

	for ( ; ; )
	{
		char charNow = PeekInput();

		switch ( stateNow )
		{

		case TokeniserState::NewToken:
			{
				uTokenStart = _inputPos;
				sCurrentToken.clear();
				bDecimalPointFound = false;
				tokCurrent = { _inputPos, Token::Type::Unknown, "" };

				//
				// -- First Character Analysis

				// End of input?
				if ( charNow == 0 )
				{
					if ( uParenthesisBalance != 0 )
					{
						throw CompilerError( CompilerError::Stage::Parser, "Parenthesis '(' & ')' not balanced" );
					}

#if DEBUG_OUTPUT_TOKENS
					// debug tokens
					std::cout << "\n-------- Tokens ------------\n";
					for ( const auto& token : vecOutputTokens )
					{
						std::cout << token.str() << "\n";
					}
					std::cout << "----------------------------\n\n";
#endif // DEBUG_OUTPUT_TOKENS

					return vecOutputTokens;
				}

				// White space?
				else if ( gWhitespaceDigits.at( charNow ) )
				{
					// Just consume, do nothing
					NextInput();
					stateNext = TokeniserState::NewToken;
				}

				// Check for Numeric Literals
				else if ( gFirstNumericDigits.at( charNow ) )
				{
					// A numeric literal has been found
					sCurrentToken = charNow;

					if ( charNow == '0' )
					{
						// Hex (0x) or Binary (0b) maybe?
						stateNext = TokeniserState::PrefixedNumericLiteral;
					}
					else
					{
						// Base 10
						stateNext = TokeniserState::NumericLiteral;
					}

					NextInput();

					bDecimalPointFound = false;
				}

				// Check for Operators
				else if ( gOperatorDigits.at( charNow ) )
				{
					stateNext = TokeniserState::Operator;

					// note: we don't consume the character here.
				}

				// Check for parenthesis
				else if ( charNow == '(' )
				{
					stateNext = TokeniserState::Parenthesis_Open;
				}
				else if ( charNow == ')' )
				{
					stateNext = TokeniserState::Parenthesis_Close;
				}

				// Unknown - presumably a symbol
				else if ( gUnitDigits.at( charNow ) )
				{
					sCurrentToken = charNow;
					NextInput();
					stateNext = TokeniserState::Unit_or_Symbol;
				}

				else
				{
					throw CompilerError( CompilerError::Stage::Parser, "Unknown character '" + std::string( 1, charNow ) + "'" );
				}


			}
			break;

		case TokeniserState::NumericLiteral:
			{
				if ( gAdditionalNumericDigits.at( charNow ) || ( charNow == _localeDecimalPoint ) )
				{
					if ( gFirstNumericDigits.at( charNow ) == false ) // If it's in Additional and not in First, it's a delimiter.
					{
						charNow = _localeDecimalPoint; // but ensure it's the kind that our locale / std::stod wants

						if ( bDecimalPointFound )
						{
							// Error ! we can only have one
							throw CompilerError( CompilerError::Stage::Parser, "Bad numeric construction" );
						}
						else
						{
							// We allow one
							bDecimalPointFound = true;
						}
					}

					sCurrentToken += charNow;
					NextInput();
					stateNext = TokeniserState::NumericLiteral;
				}
				else
				{
					// Anything else found indicates the end of this numeric literal.

					tokCurrent = { uTokenStart, Token::Type::Literal_Numeric, sCurrentToken, std::stod( sCurrentToken ) };

					stateNext = TokeniserState::CompleteToken; // check for implied addition.
				}
			}
			break;

		case TokeniserState::PrefixedNumericLiteral:
			{
				if ( charNow == 'x' || charNow == 'X' )
				{
					// Hexadecimal
					sCurrentToken += charNow;
					sPrefixedNumericLiteralToken.clear();
					NextInput();
					stateNext = TokeniserState::HexNumericLiteral;
				}

				else if ( charNow == 'b' || charNow == 'B' )
				{
					// Binary
					sCurrentToken += charNow;
					sPrefixedNumericLiteralToken.clear();
					NextInput();
					stateNext = TokeniserState::BinNumericLiteral;
				}

				else
				{
					// False alarm - treat it like a plain old number
					stateNext = TokeniserState::NumericLiteral;
				}
			}
			break;

		case TokeniserState::HexNumericLiteral:
			{
				if ( gAllowedHexDigits.at( charNow ) )
				{
					sCurrentToken += charNow;
					sPrefixedNumericLiteralToken += charNow;
					NextInput();
					stateNext = TokeniserState::HexNumericLiteral;
				}
				else if ( sPrefixedNumericLiteralToken.empty() )
				{
					throw CompilerError( CompilerError::Stage::Parser, "Invalid prefixed numeric literal" );
				}
				else
				{
					// Don't consume, something else might use this
					stateNext = TokeniserState::CompleteToken;
					tokCurrent = { uTokenStart, Token::Type::Literal_Numeric, sCurrentToken };
					tokCurrent.value = double( std::stoll( sPrefixedNumericLiteralToken, nullptr, 16 ) );
				}
			}
			break;

		case TokeniserState::BinNumericLiteral:
			{
				if ( gAllowedBinaryDigits.at( charNow ) )
				{
					sCurrentToken += charNow;
					sPrefixedNumericLiteralToken += charNow;
					NextInput();
					stateNext = TokeniserState::BinNumericLiteral;
				}
				else if ( sPrefixedNumericLiteralToken.empty() )
				{
					throw CompilerError( CompilerError::Stage::Parser, "Invalid prefixed numeric literal" );
				}
				else
				{
					// Don't consume, something else might use this
					stateNext = TokeniserState::CompleteToken;
					tokCurrent = { uTokenStart, Token::Type::Literal_Numeric, sCurrentToken };
					tokCurrent.value = double( std::stoll( sPrefixedNumericLiteralToken, nullptr, 2 ) );
				}
			}
			break;

		case TokeniserState::Operator:
			{
				// Operator matching is GREEDY
				if ( gOperatorDigits.at( charNow ) )
				{
					// If we hypothetically continue to grow the operator, is it still valid?
					if ( _mapOperators.contains( sCurrentToken + charNow ) )
					{
						// YES - keep going, nom nom nom
						sCurrentToken += charNow;
						NextInput();
					}
					else
					{
						// NO - uhm, is what we have already a valid operator?!
						if ( _mapOperators.contains( sCurrentToken ) )
						{
							// YES - bank it, we're done.
							tokCurrent = { uTokenStart, Token::Type::Operator, sCurrentToken };
							stateNext = TokeniserState::CompleteToken;
						}
						else
						{
							// NO - current operator is invalid, BUT it might be later.
							sCurrentToken += charNow;
							NextInput();
						}
					}
				}
				else
				{
					// We've left the valid operator alphabet now.
					// Let's check what we have accumulated.
					if ( _mapOperators.contains( sCurrentToken ) )
					{
						tokCurrent = { uTokenStart, Token::Type::Operator, sCurrentToken };
						stateNext = TokeniserState::CompleteToken;
					}
					else
					{
						throw CompilerError( CompilerError::Stage::Parser, "Unknown operator: " + sCurrentToken );
					}
				}
			}
			break;

		case TokeniserState::Unit_or_Symbol:
			{
				if ( gUnitDigits.at( charNow ) )
				{
					sCurrentToken += charNow;
					NextInput();
				}
				else
				{
					if ( _mapUnits.contains( sCurrentToken ) )
					{
						tokCurrent = { uTokenStart, Token::Type::Unit, sCurrentToken };
						tokCurrent.value = _mapUnits[ sCurrentToken ].scale;
					}
					else
					{
						tokCurrent = { uTokenStart, Token::Type::Symbol, sCurrentToken };
					}

					stateNext = TokeniserState::CompleteToken;
				}
			}
			break;

		case TokeniserState::Parenthesis_Open:
			{
				sCurrentToken += charNow;
				NextInput();
				++uParenthesisBalance;
				tokCurrent = { uTokenStart, Token::Type::Parenthesis_Open, sCurrentToken };
				stateNext = TokeniserState::CompleteToken;
			}
			break;

		case TokeniserState::Parenthesis_Close:
			{
				if ( uParenthesisBalance == 0 )
				{
					throw CompilerError( CompilerError::Stage::Parser, "Parenthesis '(' & ')' not balanced" );
				}

				sCurrentToken += charNow;
				--uParenthesisBalance;
				tokCurrent = { uTokenStart, Token::Type::Parenthesis_Close, sCurrentToken };
				NextInput();
				stateNext = TokeniserState::CompleteToken;
			}
			break;

		case TokeniserState::CompleteToken:
			{
				// Emit token
				vecOutputTokens.push_back( tokCurrent );
				tokPrevious = tokCurrent;
				stateNext = TokeniserState::NewToken;
			}
			break;

		}; // switch ( stateNow )

		stateNow = stateNext;

	}; // for ( ; ; )
}

Numeric::Solution Numeric::Compiler::Solve( const std::vector<Token>& vTokens, const Solution* pPrevSolution )
{
	// Solve like a calculator the stream of parsed tokens, using the Shunting Yard Algorithm
	std::deque<Token> stkHolding;
	std::deque<Token> stkOutput;

	Token tokPrevious = { 0, Token::Type::Literal_Numeric };
	int pass = 0;
	bool bExplicitUnits = false;

	for ( const auto& token : vTokens )
	{
#if DEBUG_OUTPUT_RPN_EXTRA
		// verbose debug of reverse-polish notation
		std::cout << "-------- next token --------\n";
		std::cout << token.str() << "\n";
		std::cout << "-------- RPN (holding) -----\n";
		for ( const auto& s : stkHolding )
		{
			std::cout << s.str() << "\n";
		}
		std::cout << "-------- RPN (output) ------\n";
		for ( const auto& s : stkOutput )
		{
			std::cout << s.str() << "\n";
		}
		std::cout << "----------------------------\n\n";
#endif // DEBUG_OUTPUT_RPN_EXTRA

		if ( token.type == Token::Type::Literal_Numeric )
		{
			// Push literals straight to output, they are already in order
			stkOutput.push_back( token );
			tokPrevious = stkOutput.back();
		}
		else if ( token.type == Token::Type::Parenthesis_Open )
		{
			// Push to holding stack, it acts as a stopper when we back track
			stkHolding.push_front( token );
			tokPrevious = stkHolding.front();
		}
		else if ( token.type == Token::Type::Parenthesis_Close )
		{
			// Check something is actually wrapped by parenthesis
			if ( stkHolding.empty() )
			{
				throw CompilerError( CompilerError::Stage::Solver, "Unexpected close parenthesis" );
			}

			// Back-flush holding stack into output until open parenthesis
			while ( !stkHolding.empty() && stkHolding.front().type != Token::Type::Parenthesis_Open )
			{
				stkOutput.push_back( stkHolding.front() );
				stkHolding.pop_front();
			}

			// Check if open parenthesis was actually found
			if ( stkHolding.empty() )
			{
				throw CompilerError( CompilerError::Stage::Solver, "No open parenthesis found" );
			}

			// Remove corresponding open parenthesis from holding stack
			if ( !stkHolding.empty() && stkHolding.front().type == Token::Type::Parenthesis_Open )
			{
				stkHolding.pop_front();
			}

			tokPrevious = { 0, Token::Type::Parenthesis_Close };
		}

		else if ( token.type == Token::Type::Unit )
		{
			bExplicitUnits = true; // units were specified

			// Push units straight to output, they are already in order
			stkOutput.push_back( token );
			tokPrevious = stkOutput.back();
		}

		else if ( token.type == Token::Type::Operator )
		{
			// Unit_or_Symbol is operator
			std::string op_text = token.text;

			// Unary Operator check
			if ( token.text == "-" || token.text == "+" )
			{
				if ( ( tokPrevious.type != Token::Type::Literal_Numeric
					   && tokPrevious.type != Token::Type::Unit
					   && tokPrevious.type != Token::Type::Parenthesis_Close ) || pass == 0 )
				{
					// "Upgrade" operator
					op_text = "u" + token.text;
				}
			}

			while ( !stkHolding.empty() && stkHolding.front().type != Token::Type::Parenthesis_Open )
			{
				// Ensure holding stack front is an operator (it might not be later...)
				if ( stkHolding.front().type == Token::Type::Operator )
				{
					const auto& holding_stack_op = _mapOperators[ stkHolding.front().text ];

					if ( holding_stack_op.precedence >= _mapOperators[ op_text ].precedence )
					{
						stkOutput.push_back( stkHolding.front() );
						stkHolding.pop_front();
					}
					else
					{
						break;
					}
				}
			}

			// Push the new operator onto the holding stack
			stkHolding.push_front( { 0, Token::Type::Operator, op_text } );
			tokPrevious = stkHolding.front();
		}

		else
		{
			throw CompilerError( CompilerError::Stage::Solver, "Unsupported Token" );
		}

		pass++;
	}

	// Drain the holding stack
	while ( !stkHolding.empty() )
	{
		stkOutput.push_back( stkHolding.front() );
		stkHolding.pop_front();
	}

#if DEBUG_OUTPUT_RPN
	// debug reverse-polish notation
	std::cout << "-------- RPN ------------\n";
	for ( const auto& s : stkOutput )
	{
		std::cout << s.str() << "\n";
	}
	std::cout << "-------------------------\n\n";
#endif // DEBUG_OUTPUT_RPN

	// Solver (Almost identical to video 1)
	std::deque<Solution> stkSolve;

	for ( const auto& inst : stkOutput )
	{
		switch ( inst.type )
		{
		case Token::Type::Literal_Numeric:
			{
				stkSolve.push_front( { inst.value, 1.0 } );
			}
			break;

		case Token::Type::Unit:
			{
				Solution mem;

				if ( stkSolve.empty() )
				{
					throw CompilerError( CompilerError::Stage::Solver, "Expression is malformed" );
				}
				else
				{
					mem = stkSolve[ 0 ];
					stkSolve.pop_front();
				}

				Solution result;
				result.value = mem.value * inst.value;
				result.units = _mapUnits[ inst.text ];

				stkSolve.push_front( result );
			}
			break;

		case Token::Type::Operator:
			{
				const auto& op = _mapOperators[ inst.text ];

				std::vector<Solution> mem( op.arguments );

				for ( uint8_t a = 0; a < op.arguments; a++ )
				{
					if ( stkSolve.empty() )
					{
						throw CompilerError( CompilerError::Stage::Solver, "Expression is malformed" );
					}
					else
					{
						mem[ a ] = stkSolve[ 0 ];
						stkSolve.pop_front();
					}
				}

				Solution result = { 0.0, { 1.0, UnitType::Generic } };

				if ( op.arguments == 2 )
				{
					if ( inst.text == "/" )
					{
						double v0, v1;
						v0 = mem[ 0 ].value / mem[ 0 ].units.scale;
						v1 = mem[ 1 ].value / mem[ 1 ].units.scale;

						result.value = v1 / v0;

						if ( mem[ 0 ].units.type != UnitType::Generic )
						{
							result.units = mem[ 0 ].units;
							result.value *= mem[ 0 ].units.scale;
						}
						else
						{
							result.units = { 1.0, _desiredUnitType };
						}
					}
					else if ( inst.text == "*" )
					{
						result.value = mem[ 1 ].value * mem[ 0 ].value;
						result.units = { 1.0, _desiredUnitType };
					}
					else if ( inst.text == "+" )
					{
						double v0, v1;
						v0 = mem[ 0 ].value / mem[ 0 ].units.scale;
						v1 = mem[ 1 ].value / mem[ 1 ].units.scale;

						if ( mem[ 0 ].units.type == UnitType::Generic )
						{
							result.units = mem[ 1 ].units;
							result.value = ( v1 + v0 ) * mem[ 1 ].units.scale;
						}
						else if ( mem[ 1 ].units.type == UnitType::Generic )
						{
							result.units = mem[ 0 ].units;
							result.value = ( v1 + v0 ) * mem[ 0 ].units.scale;
						}
						else
						{
							result.value = mem[ 1 ].value + mem[ 0 ].value;
						}
					}
					else if ( inst.text == "-" )
					{
						double v0, v1;
						v0 = mem[ 0 ].value / mem[ 0 ].units.scale;
						v1 = mem[ 1 ].value / mem[ 1 ].units.scale;

						if ( mem[ 0 ].units.type == UnitType::Generic )
						{
							result.units = mem[ 1 ].units;
							result.value = ( v1 - v0 ) * mem[ 1 ].units.scale;
						}
						else if ( mem[ 1 ].units.type == UnitType::Generic )
						{
							result.units = mem[ 0 ].units;
							result.value = ( v1 - v0 ) * mem[ 0 ].units.scale;
						}
						else
						{
							result.value = mem[ 1 ].value + mem[ 0 ].value;
						}
					}
				}

				if ( op.arguments == 1 )
				{
					if ( inst.text == "u+" )
					{
						result = mem[ 0 ];
						result.units = mem[ 0 ].units;
					}
					else if ( inst.text == "u-" )
					{
						result.value = -mem[ 0 ].value;
						result.units = mem[ 0 ].units;
					}
				}

				stkSolve.push_front( result );
			}
			break;

		default:
			throw CompilerError( CompilerError::Stage::Solver, "Unexpected Token" );
		}
	}

	if ( stkSolve.size() != 1 )
	{

#if DEBUG_OUTPUT_ERROR
		std::cout << "Solution  := \n";
		for ( const auto& s : stkSolve )
		{
			std::cout << std::to_string( s.value ) << " (x" << std::to_string( s.units.scale ) << ")" << "\n";
		}
		std::cout << "\n";
#endif // DEBUG_OUTPUT_ERROR

		throw CompilerError( CompilerError::Stage::Solver, "Indeterminate Expression" );
	}
	else
	{
		Solution result = stkSolve.front();

		// No units were explicitly specified?
		if ( bExplicitUnits == false )
		{
			if ( pPrevSolution == nullptr || pPrevSolution->units.type == UnitType::Generic )
			{
				// fall back to desired units
				result.units = { 1.0, _desiredUnitType };
				result.value *= result.units.scale;
			}
			else
			{
				// recycle the previous solution's units
				result.value *= pPrevSolution->units.scale;
				result.units = pPrevSolution->units;
			}
		}

		// Convert?
		if ( result.units.type != _desiredUnitType )
		{
			result.units = { 1.0, _desiredUnitType };
		}

		// Try to normalise the result into friendly values.
		if ( _desiredUnitType == UnitType::Imperial )
		{
			NormaliseImperial( result );
		}
		else if ( _desiredUnitType == UnitType::Metric )
		{
			NormaliseMetric( result );
		}

		return result;
	}
}

const std::string Numeric::Compiler::UnitName( const Unit& unit ) const
{
	if ( unit.type == UnitType::Generic )
	{
		return std::string( "" );
	}

	const auto find = _mapUnitLookup.find( unit.scale );
	if ( find == _mapUnitLookup.end() )
	{
		return std::string( "<error>" );
	}

	return ( *find ).second;
}

void Numeric::Compiler::NormaliseImperial( Solution& result )
{
	// .. zero?
	if ( result.value == 0 )
	{
		result.units = DefaultUnit();
		return;
	}

	for ( ; ; )
	{
		double normalised = std::fabs( result.value / result.units.scale );

		// ... convert from in to ft, if it doesn't create a fraction.
		if ( ( normalised >= _impScaleInch/*1*/ ) && ( result.units.scale == _impScaleThou ) )
		{
			result.units.scale = _impScaleInch;
		}
		// ... convert from in to ft, don't care about fractions if the value is (>6ft)
		else if ( ( normalised > 72 ) && ( result.units.scale == _impScaleInch ) )
		{
			result.units.scale = _impScaleFoot;
		}
		// ... convert from in to ft, if it doesn't create a fraction.
		else if ( ( normalised >= 12 ) && ( result.units.scale == _impScaleInch ) && ( IsEpsilonInteger( result.value / _impScaleFoot ) ) )
		{
			result.units.scale = _impScaleFoot;
		}
#if OUTPUT_TO_YARDS
		// ... convert from ft to yd, if it doesn't create a fraction.
		else if ( ( normalised >= 12 ) && ( result.units.scale == _impScaleFoot ) && ( IsEpsilonInteger( result.value / _impScaleYard ) ) )
		{
			result.units.scale = _impScaleYard;
		}
#else // OUTPUT_TO_YARDS
		// ... convert from yd to feet
		else if ( result.units.scale == _impScaleYard )
		{
			result.units.scale = _impScaleFoot;
		}
#endif // OUTPUT_TO_YARDS
		// ... convert from ft to mi, if it doesn't create a fraction.
		else if ( ( normalised >= 5280 ) && ( result.units.scale == _impScaleFoot ) && ( IsEpsilonInteger( result.value / _impScaleMile ) ) )
		{
			result.units.scale = _impScaleMile;
		}
		// ... convert from yd to mi, if it doesn't create a fraction.
		else if ( ( normalised >= 1760 ) && ( result.units.scale == _impScaleYard ) && ( IsEpsilonInteger( result.value / _impScaleMile ) ) )
		{
			result.units.scale = _impScaleMile;
		}
		else
		{
			break; // nothing more we can do.
		}
	}
}

void Numeric::Compiler::NormaliseMetric( Solution& result )
{
	// .. zero?
	if ( result.value == 0 )
	{
		result.units = DefaultUnit();
		return;
	}

	for ( ; ; )
	{
		double normalised = std::fabs( result.value / result.units.scale );

		// ... convert from km to Mm
		if ( ( normalised >= 1000 ) && ( result.units.scale == 1000 ) )
		{
			result.units.scale = 1000000;
		}
		// ... convert from m to km
		else if ( ( normalised >= 1000 ) && ( result.units.scale == 1 ) )
		{
			result.units.scale = 1000.0;
		}
		// ... convert from mm to m
		else if ( ( normalised >= 1000 ) && ( result.units.scale == 0.001 ) )
		{
			result.units.scale = 1;
		}
#if OUTPUT_TO_CM
		// ... convert from mm to cm
		else if ( ( normalised >= 100 ) && ( result.units.scale == 0.001 ) )
		{
			result.units.scale = 0.01;
		}
		// ... convert from cm to m
		else if ( ( normalised >= 100 ) && ( result.units.scale == 0.01 ) )
		{
			result.units.scale = 1;
		}
#else // OUTPUT_TO_CM
		// ... convert from cm to m
		else if ( result.units.scale == 0.01 )
		{
			result.units.scale = 1;
		}
		// ... convert from mm to m
		else if ( ( normalised >= 1000 ) && ( result.units.scale == 0.001 ) )
		{
			result.units.scale = 1;
		}
#endif // OUTPUT_TO_CM
		// ... convert from Mm to km
		else if ( ( normalised < 1 ) && ( result.units.scale == 1000000 ) )
		{
			result.units.scale = 1000;
		}
		// ... convert from km to m
		else if ( ( normalised < 1 ) && ( result.units.scale == 1000 ) )
		{
			result.units.scale = 1;
		}
#if OUTPUT_TO_CM
		// ... convert from m to cm
		else if ( ( normalised < 1 ) && ( result.units.scale == 1 ) )
		{
			result.units.scale = 0.01;
		}
		// ... convert from cm to mm
		else if ( ( normalised < 1 ) && ( result.units.scale == 0.01 ) )
		{
			result.units.scale = 0.001;
		}
#else // OUTPUT_TO_CM
		// ... convert from m to mm
		else if ( ( normalised < 1 ) && ( result.units.scale == 1 ) )
		{
			result.units.scale = 0.001;
		}
#endif // OUTPUT_TO_CM
		else
		{
			break; // nothing more we can do.
		}
	}
}

void Numeric::Compiler::SetupInput( const std::string& input )
{
	_inputStream = input.begin();
	_inputStreamEnd = input.end();
	_inputPos = 0;
}

char Numeric::Compiler::PeekInput()
{
	char charNow;

	if ( _inputStream == _inputStreamEnd )
	{
		charNow = 0;
	}
	else
	{
		charNow = *_inputStream;
	}

	return charNow;
}

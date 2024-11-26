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

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <deque>
#include <array>

namespace Numeric
{
	namespace lut
	{
		static constexpr std::array<bool, 256> MakeLUT( const std::string_view sAcceptedCharacters )
		{
			std::array<bool, 256> lut{ 0 };
			for ( const auto c : sAcceptedCharacters )
			{
				lut.at( uint8_t( c ) ) = true;
			}

			return lut;
		}
	}

	enum class UnitType
	{
		Generic,
		Metric,
		Imperial,
	};

	struct Unit
	{
		double scale = 1.0;
		UnitType type = UnitType::Generic;
	};

	struct Solution
	{
		double value;
		Unit units;
	};

	struct Token
	{
		enum class Type
		{
			Unknown,
			Literal_Numeric,
			Operator,
			Parenthesis_Open,
			Parenthesis_Close,
			Symbol,
			Unit,
		};

		size_t pos = 0;
		Type type = Type::Unknown;
		std::string text = "";
		double value = 0.0;

	public:
		std::string str() const;
	};

	struct Operator
	{
		int precedence = 0;
		int arguments = 0;
	};

	class CompilerError : public std::exception
	{
	public:

		enum class Stage
		{
			Parser,
			Solver
		};

		CompilerError( Stage stage, const std::string& sMsg );

	public:
		const char* what() const override
		{
			return _message.c_str();
		}

	private:
		std::string _message;
	};

	class Compiler
	{

	public:
		Compiler();
		virtual ~Compiler() {}

	public: // configuration
		void SetUnitOut( const UnitType type );
		void SetImperialFractions( bool enable ) { _imperialFractions = enable; }

	public: // general use
		Solution Eval( const std::string& sInput, const Solution* pPrevSolution );
		const std::string Format( const Solution& result ) const;

	public: // low level access
		const Unit DefaultUnit() const;
		std::vector<Token> Parse( const std::string& sInput );
		Solution Solve( const std::vector<Token>& vTokens, const Solution* pPrevSolution );

	private:

		static bool IsEpsilonInteger( double d )
		{
			const double delta = d - round( d );
			return fabs( delta ) <= 1e-14;
		}

		const std::string UnitName( const Unit& unit ) const;
		void NormaliseImperial( Solution& result );
		void NormaliseMetric( Solution& result );

	private:

		void SetupInput( const std::string& input );
		char PeekInput();

		inline void NextInput()
		{
			++_inputStream;
			++_inputPos;
		}

		std::string::const_iterator _inputStream;
		std::string::const_iterator _inputStreamEnd;
		size_t _inputPos = 0;

	private:

		static constexpr double _metricScaleInch = 0.0254;
		static constexpr double _metricScaleFoot = 0.3048;
		static constexpr double _metricScaleYard = 0.9144;
		static constexpr double _metricScaleMile = 1609.344;

		static constexpr double _impScaleThou = 1;
		static constexpr double _impScaleInch = 1000;
		static constexpr double _impScaleFoot = 12 * _impScaleInch;
		static constexpr double _impScaleYard = 3 * _impScaleFoot;
		static constexpr double _impScaleMile = 5280 * _impScaleFoot;

	private:

		char _localeDecimalPoint = '.';

		bool _imperialFractions = true;
		
		std::unordered_map<std::string, Operator> _mapOperators;

		// units tables
		UnitType _desiredUnitType = UnitType::Metric;
		std::unordered_map<std::string, Unit> _mapUnits;
		std::unordered_map<double, std::string> _mapUnitLookup;

	}; // class Compiler

}; // namespace Numeric


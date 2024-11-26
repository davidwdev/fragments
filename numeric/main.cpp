
// this file specifically is licensed under CC0. see: https://creativecommons.org/public-domain/cc0/

#include <iostream>
#include <string>
#include <locale>

#include "numeric.h"

int main()
{
	std::locale::global( std::locale( "" ) ); // apply the locale from the environment

	std::cout << "\n==========================================\n";
	std::cout << "=== Smart Numeric 'Edit Box' Simulator ===\n";
	std::cout << "==========================================\n";

	std::cout << "\n--- Scenario ------\n";
	std::cout << "You are using expensive CAD software. Suddenly! You are presented with\n";
	std::cout << "a popup asking for a position. Dare you type something in, maybe a sum?\n";
	std::cout << "Use metric or imperial units or both! Show them ALL that you're a PRO!\n";
	std::cout << "The edit box never tires. It hungers for new values.\n";
	std::cout << "\n--- Instructions ------\n";
	std::cout << "Enter \"metric\" to use the Metric system (default).\n";
	std::cout << "Enter \"imperial\" to use the Imperial system.\n";
	std::cout << "Enter \"generic\" to use generic units.\n";
	std::cout << "Enter a blank line to return to more tedious activities.\n";

	Numeric::Compiler compiler;
	compiler.SetUnitOut( Numeric::UnitType::Metric );

	Numeric::Solution prevSolution = { 0 };
	prevSolution.units = compiler.DefaultUnit();

	for ( ; ; )
	{
		std::cout << "\nInput > ";

		std::string sExpression;
		std::getline( std::cin, sExpression );

		// .,. blank line to exit.
		if ( sExpression.empty() )
		{
			break;
		}
		else if ( sExpression == "imperial" )
		{
			compiler.SetUnitOut( Numeric::UnitType::Imperial );
			std::cout << "System units were set to Imperial\n";
			prevSolution.value = 0;
			prevSolution.units = compiler.DefaultUnit();
		}
		else if ( sExpression == "metric" )
		{
			compiler.SetUnitOut( Numeric::UnitType::Metric );
			std::cout << "System units were set to Metric\n";
			prevSolution.value = 0;
			prevSolution.units = compiler.DefaultUnit();
		}
		else if ( sExpression == "generic" )
		{
			compiler.SetUnitOut( Numeric::UnitType::Generic );
			std::cout << "System units were set to Generic\n";
			prevSolution.value = 0;
			prevSolution.units = compiler.DefaultUnit();
		}
		else
		{
			try
			{
				prevSolution = compiler.Eval( sExpression, &prevSolution );

				std::cout << "\nThe edit box shows: " << compiler.Format( prevSolution ) << "\n";
			}
			catch ( Numeric::CompilerError& e )
			{
#if 1
				(void)e;
				std::cout << " - Error.\n";
#else
				std::cout << "*** ERROR *** " << e.what() << "\n"; // verbose
#endif
			}
		}
	}

	return 0;
}
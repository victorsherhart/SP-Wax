
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
* * * * * *                  Coutinho                 * * * * * *
* * * * * *          Solid-liquid Equilibrium         * * * * * *
* * * * * *              Thermodynamic Model          * * * * * *
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* In this source file, set of K-Values are generated for temperatures
starting from 280.15K to WAT which are used 
by other source and exe-files. In addition, total number of K-Values are 
reported in seperate textfile*/

//Used libraries
#include <iostream>
#include <cmath>
#include <fstream>
#include <time.h>
#include "Header.h"
#include <iomanip> 
#include "omp.h"
#include <windows.h>
#include <string>
using namespace std;

//Function to get current directory
string ExeDir() {
	char Var[MAX_PATH];
	GetModuleFileName(NULL, Var, MAX_PATH);
	string::size_type pos = string(Var).find_last_of("\\/");
	return string(Var).substr(0, pos);
}

//Global Variables
//Number of threads
const int NumThreads = 8;

//Ideal gas constant
const double R = 8.3144621; // J/Mol/K -All units are in SI 

int main()
{
	/********************************Inputs*************************************/
	string Directory = ExeDir();
	//Paraffin weight fraction in the oil sample
	double NalkaneWeightFraction = 0.147739;

	//Correction factor coefficient 
	double CF;

	//Total number of components (n-alkanes)
	int CompNum;

	//Minimum carbon number which is desired to be included in the calculations 
	int MinC;

	//Number of temperatures
	int NumTemp;

	//First srtarting temperature 
	double StartTemp;

	//Maximum acceptable Error
	double Epsilon;

	//Temperature step
	double TempInterval;


	/***************************End of Inputs********************************/

	//Reading files
	ifstream CompositionNalkanes, PrecipitatationCurveWAT, KInput,
		GeneralInputs;
	GeneralInputs.open(Directory + "\\GeneralInputs.txt");
	PrecipitatationCurveWAT.open(Directory + "\\PrecipitatationCurveWAT.txt");
	CompositionNalkanes.open(Directory + "\\Data.txt");
	KInput.open(Directory + "\\KInput.txt");

	//Output files
	ofstream nSFile, NumLine, KInitialVal;
	KInitialVal.open(Directory + "\\KInitialVal.txt");
	NumLine.open(Directory + "\\NumLine.txt");

	//Number of threads to be used
	int nthreads;

	//Array to store the error for each component
	double sumErr[100 + 1];

	/*Normalized composition means summation of all carbon numbers should
	be equal to one*/
	clock_t t;

	//Input n-alkane mole fractions (summation of all mole fractions is equal to 1)
	double Z[100 + 1];

	/*Total weight of n-alkanes if it is assumed that all components are
	in liquid phase and summation of their mole fractions is one */
	double  SumWOil;

	/*Total weight of solid phase in n-alkane system if it is assumed
	that total n-alkanes equal to one mole */
	double	SumW;

	//Normalized solid phase molar composition 
	double	XS[100 + 1];

	//Normalized Liquid phase molar composition
	double	XL[100 + 1];

	//Equilibrium constant values for a temperature (New)
	double 	K[100 + 1];

	//Equilibrium constant values for a temperature (old)
	double	 KC[100 + 1];

	/*weight of each carbon number component if all n-alkanes are in liquid phase
	and summation of all carbon number mole fractions goes to one*/
	double 	WOil[100 + 1];

	// Error calculation between of old and new K_values 
	double Err = 1000;

	/*Mole fraction of n-alkanes in liquid phase at a certain temperature if it is
	assumed that summation of all n-alkanes (liquid+solid) is one mole*/
	double nL;

	//Temperature
	double T;

	/*Mole fraction of n-alkanes in liquid phase at a certain temperature if it is
	assumed that summation of all n-alkanes (liquid+solid) is one mole
	Initial Guess, there is not usually any convergence problem associated with 
	the initial guess of nS*/
	double nS = 0.0000220462;

	//Molecular weight (Local variable)
	double Mw;

	//Selected temperatures
	double Temp[200];

	//carbon number identification or CN
	int CarbonNum[100 + 1];

	//2D array to save Solid composition of all temperatures 
	double SolidCompW[200][100 + 1];

	//Number of assigned threads
	omp_set_num_threads(NumThreads);

	//Initial input K-values for THIS FILE
	double KIn[100 + 1];

	//Local usefull variables (no certain meaning)
	double	a;
	double Number = 0; //Number of K-Values generated by this source file
	double SumWS = 0;
	double nSGuess;
	int Click = 0;

	//calling DIPPR molar volume coefficients
	ArrayRetA();
	ArrayRetB();
	ArrayRetC();
	ArrayRetD();

	//Assign variables from the text files
	GeneralInputs >> NalkaneWeightFraction;
	NalkaneWeightFraction = NalkaneWeightFraction / 100.0;
	GeneralInputs >> CF;
	GeneralInputs >> MinC;
	GeneralInputs >> CompNum;
	NumTemp=100;
	StartTemp=275.15;
	TempInterval=0.5;
	Epsilon= 0.0001;

	SumWOil = 0;
	//In this for loop, different variables are assgined
	for (int i = 1; i < CompNum + 1; i++)
	{
		//Assigning carbon number identification
		/* Assigning carbon numbers to the array. CarbonNum[] is used as an input
		in many functions*/
		CarbonNum[i] = i;

		//Reading compositions from the text files & put them to the assigned arrays
		CompositionNalkanes >> Z[i];

		//Calculating weight of each carbon number component
		WOil[i] = Z[i] * (12 * CarbonNum[i] + 2 * CarbonNum[i] + 2);

		//Total weight calculations
		SumWOil = SumWOil + WOil[i];
	}

	/*This "for loop" is to give initial guess values for equilibrium constant
	for non-participating carbon number components*/
	for (int i = 1; i < MinC; i++) {
		K[i] = 0.0;
		KC[i] = 0.0;
	}

	/*Initial K-values for this files.
	Please note that KIn is only applicable for lower temperatures*/
	for (int i = 1; i < CompNum + 1; i++) 
	{
		KInput >> KIn[i];
	}

	/*Temperature based initial values for equilibrium constants for participating
	n-alkane components*/
	for (int i = MinC; i < CompNum + 1; i++)
	{
		K[i] = KIn[i];
	}


	//Based on the input data (from user), temperatures will be assigned
	for (int i = 0; i < NumTemp; i++)
	{
		Temp[i] = StartTemp + TempInterval * i;
	}

	//The parameters which will be displayed at time of simulation run
	cout << "Temperature" << setw(4) << ' ' << "execution time" << setw(3)
		<< ' ' << "wax weight fraction" << endl;

	/*This "for loop" iterates the temperature while all calculated properties
	will be saved*/
	for (int j = 0; j < NumTemp; j++)
	{
		T = Temp[j];
		t = clock();

		/*This while loop makes sure nS is optimally chosen and all equilibrium
		constants have been chosen correctly*/
		while (abs(Err) > Epsilon)
		{
			//Based on given K-values, nS is picked through the following function 
			nS = Fsolve(nS, CompNum, Z, K);
			nL = 1 - nS;
			Click = 0;
			//Based on the chosen nS, following parameters are calculated
			for (int i = 1; i < CompNum + 1; i++)
			{
				//This if-statement takes excludese components with zero composition
				if (Z[i] == 0)
				{
					XL[i] = 0;
					XS[i] = 0;
				}
				else
				{
					/*Based on the newly calculated nS, it is possible to calculate the
					liquid and solid composition of n-Alkanes*/
					XL[i] = Z[i] / (1 + nS * (K[i] - 1));
					XS[i] = Z[i] * K[i] / (1 + nS * (K[i] - 1));
					if (XS[i] >(pow(10, -6)) && Click == 0) {
						Click = 1;
						MinC = i;
					}
					/*It saves the previous K values, so they can be compared with the new
					ones which will be estimated in the following section.*/
					KC[i] = K[i];
				}

			}

			//New equilibrium constants are calculated for the selected temperature
#pragma omp parallel for
			for (int i = MinC; i < CompNum + 1; i++)
			{
				if (Z[i] != 0)
				{
					K[i] = (GammaLiqMesEFV(MinC, i, CarbonNum[i], CompNum, CarbonNum, XL, T) /
						GammaSolMes(MinC, CarbonNum[i], CompNum, CarbonNum, XS, T, CF))
						*exp(((1000 * HeatFus(CarbonNum[i]) / (R*TempFus(CarbonNum[i])))
							*(TempFus(CarbonNum[i]) / T - 1)) + ((1000 * HeatTrans(CarbonNum[i]) /
							(R*TempTrans(CarbonNum[i])))*(TempTrans(CarbonNum[i]) / T - 1)) -
								(((4.1868*(0.3033*((12 * CarbonNum[i] + CarbonNum[i] * 2 + 2)) -
									4.635*pow(10, -4)*T*((12 * CarbonNum[i] + CarbonNum[i] * 2 + 2)))) / R)
									*(TempFus(CarbonNum[i]) / T - log(TempFus(CarbonNum[i]) / T) - 1)));

					/*Difference between old and new equilibrium constants are calculated and
					saved for each carbon number*/
					sumErr[i] = abs(K[i] - KC[i]) / KC[i];
				}
				else
				{
					sumErr[i] = 0;
				}
			}

			//Total error calculation
			Err = 0;
#pragma omp parallel for reduction (+:Err)
			for (int j = MinC; j < CompNum + 1; j++)
			{
				Err = Err + sumErr[j] / (CompNum - MinC);
			}
			if (nS > 0) {
				nSGuess = nS;
			}
		}

		SumWS = 0;
		for (int i = 1; i < CompNum + 1; i++)
		{ /*This for loop, calculates the summations of all components of each phase.
		  It basically is used to check if both phase's compositinos add up to unity*/
			if (T >= 280.15)
			{
				KInitialVal << K[i] << "	";
				
			}
			SumWS = SumWS + XS[i] * (12 * CarbonNum[i] + CarbonNum[i] * 2 + 2);
		}

		if (T >= 280.15)
		{
			Number = Number + 1;
			KInitialVal << endl;
		}
		for (int i = 1; i < CompNum + 1; i++)
		{
			SolidCompW[j][i] = XS[i] * (12 * CarbonNum[i] + CarbonNum[i] * 2 + 2) / SumWS;
		}
		Err = 200;

		//To check if the right temperature is selected
		isnan(nS) ? a = 1 : 0;
		if (a == 1)
		{
			cout << " No precipitation at selected temperature " << endl;
			break;
		}

		if (nS < 0)
		{
			break;
		}

		/*Please refer to parameter definition section to know the nature of
		each paramater*/
		SumW = 0;
#pragma omp parallel for reduction (+:SumW)
		for (int i = 1; i < CompNum + 1; i++)
		{
			SumW = SumW + nS * XS[i] * (12 * CarbonNum[i] + CarbonNum[i] * 2 + 2);
			if (XS[i] >(6 * pow(10, -5)) && Click == 0)
			{
				Click = 1;
				MinC = i;
			}
		}
		t = clock() - t;

		Err = 200;

		//The parameters which will be displayed at time of simulation run
		if (nS > 0)
		{
			cout << setprecision(2) << fixed << T << setw(10) << left << " " <<
				setprecision(4) << ((float)t) / CLOCKS_PER_SEC << setw(10) << left
				<< " " << setprecision(8) << fixed << NalkaneWeightFraction * SumW
				/ SumWOil << endl;
		}
	}

	/*Total number of generated K-Values starting from T=280.15 to WAT */
	NumLine << Number;

	KInitialVal.close();
	NumLine.close();
}
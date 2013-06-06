// @ZBS {
//		+DESCRIPTION {
//			ECE Senior Deisgn Project 2010
//		}
//		*SDK_DEPENDS clapack
// }

// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zviewpoint.h"
#include "zintegrator_gsl.h"

/************************************************our code****************************************/

#include "gsl/gsl_matrix.h"
#include "gsl/gsl_vector.h"
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_blas.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <stdlib.h> 
//#include <iostream.h> 
#include <string> 
#include <cstdlib>
 #include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <iostream>
#include "netlist.h"
#include "math.h"
//#include <armadillo>

#include "zui.h"


using namespace std;
//using namespace arma;

//declaring output file stream for solution
std::ofstream solution;//declares an ofstream




//
// Initializing counter for each component
//
int pmos = 0;
int nmos = 0;
int resistor = 0;
int capacitor = 0;
int inductor = 0;
int voltage = 0;
double intReturn; 
double maxNode;

//
//Declaring string vector to store netlist

vector<string> NMOS;
vector<string> PMOS;
vector<string> RESISTOR;
vector<string> CAPACITOR;
vector<string> INDUCTOR;
vector<string> VOLTAGE;

//
//Declaring double vector to store data

vector<double> NEWPMOS;
vector<double> NEWNMOS;
vector<double> NEWCAPACITOR;
vector<double> NEWINDUCTOR;
vector<double> NEWVOLTAGE;
vector<double> NEWRESISTOR;
vector<double> NEWIVCONTROL;

//vector<int> item;

//
//Declaring iterators over each vector

vector<double>::iterator NEWPMOSiter;
vector<double>::iterator NEWNMOSiter;
vector<double>::iterator NEWCAPACITORiter;
vector<double>::iterator NEWINDUCTORiter;
vector<double>::iterator NEWVOLTAGEiter;
vector<double>::iterator NEWRESISTORiter;
vector<double>::iterator NEWIVCONTROLiter;


//Declaring variables in which to store data
string nextToken;
string temp;

//Declaring initial conditions and ODE matrix

gsl_matrix *ODEM;
gsl_vector *x0;
//gsl_matrix *x0;  
	
	



//-----------------------------------------------------------------------------------------------------
//---------------------------------GetIntVal-----------------------------------------------------

// This function converts a string to a double and is used to populate the vectors of doubles 

 double GetIntVal(string strConvert) 
 {    
   intReturn = atof(strConvert.c_str()); // atof is help full to convert string in to float :) 
   //cout<<intReturn<<endl;
  return(intReturn);   
}
//////////////////////////////////////////////////////////////////

double getval(vector<double> vec,vector<double>::iterator vecIter,int numberofcolumns,int row, int col)
{
	vecIter = vec.begin();
	vecIter = vecIter + row*numberofcolumns + col;
	return *vecIter;
}
void setval(vector<double> &vec,vector<double>::iterator &vecIter,int numberofcolumns,int row, int col, double val)
{
	//vecIter = vec.begin();
	//vecIter = vecIter + row*numberofcolumns + col;
	//vec.erase(vecIter);
	vecIter = vec.begin();
	vecIter = vecIter + row*numberofcolumns + col;
	vec.insert(vecIter,val);	
}

//--------------------------------------------------------------------------------------------------------
//---------------------------------CreateArrays--------------------------------------------------------
 // This function serves  two purposes:  (1) it creates and populates the string vectors and (2) it creates and populates the double vecotrs. 

void createMosArray ()
{
	
	build_netlist("netlist.txt");
	
	
	
	 char inchar;
	 string line;
	 ifstream inFile;
	 inFile.open("netlist.txt",ios::in);
	

	  
		
  //ifstream newFile ("test.txt"); // open two different file
  //ifstream inFile ("test.txt"); 
  int check = inFile.good();
  
	while(inFile.good())
	{
		int tok = 0;
		inchar = inFile.get();

		if (inchar == 'p' || inchar == 'P')
		{
				pmos++;				
			getline (inFile,line);
			string s = line;
			stringstream os(s);		
				
			while (os >> temp)                //the stringstream makes temp a token
				{ 
					PMOS.push_back(temp);
				}// end of while
		 	
		} // end of if statement 

		else if (inchar == 'n' || inchar == 'N')
		{
				nmos++;				
				getline (inFile,line);
				string s = line;
				stringstream os(s);     
				while (os >> temp)                //the stringstream makes temp a token
				{ 
					NMOS.push_back(temp);
				}// end of while
		} // end of else if 
		else if ( inchar == 'r' || inchar == 'R')
		{
			resistor++;
			getline (inFile,line);
			string s = line;
			stringstream os(s);     
			while (os >> temp)                //the stringstream makes temp a token
			{ 
				RESISTOR.push_back(temp);
			}// end of while			
		}// end of else if for (r or R)
		else if (inchar == 'c' || inchar == 'C')
		{
			capacitor++;
			getline (inFile,line);
			string s = line;
			stringstream os(s);     
			while (os >> temp)                //the stringstream makes temp a token
			{ 
				CAPACITOR.push_back(temp);
			}// end of while
		}// end of else if for (c or C)
		else if (inchar == 'l' || inchar == 'L' )
		{
			inductor++;
			getline (inFile,line);
			string s = line;
			stringstream os(s);     
			while (os >> temp)                //the stringstream makes temp a token
			{ 
				INDUCTOR.push_back(temp);
			}// end of while
		}// end of else if for (l or L)
		else if (inchar == 'V' || inchar == 'v')
		{
			voltage++;
			getline (inFile,line);
			string s = line;
			stringstream os(s);     
			while (os >> temp)                //the stringstream makes temp a token
			{ 
				VOLTAGE.push_back(temp);
			}// end of while
		}//end of else if for VOLtage
	}// end of while loop */
	inFile.close();

   
	cout << "Number of PMOS in your netlist is = " << pmos << endl;
	cout << "Number of NMOS in your netlist is = " << nmos << endl;
	cout << "Your PMOS size of array is = " << PMOS.size() << endl;
	cout << "Your NMOS size of array is = " << NMOS.size() << endl;
	
	cout << " C = " << capacitor << endl;
    cout << " r = " << resistor << endl;
	cout << " l = " << inductor << endl;
	cout << " v = " << voltage << endl;

	cout << " size of C ARRAY = " << CAPACITOR.size() << endl ;
	cout << " size of R ARRAY = " << RESISTOR.size() << endl ;
	cout << " size of L ARRAY = " << INDUCTOR.size() << endl ;

}// end of void createMosArray

//---------------------------------GetIntVal-----------------------------------------------------

 //--------------------------------------PMOSEconverter...........................................
 
 void VectorConv()
 {
//................................CONVERTING PMOS.............................................	 
	vector<string>::iterator PMOSiter;
	for ( PMOSiter = PMOS.begin(); PMOSiter < PMOS.end(); PMOSiter++ )
	{	
		 string str = *PMOSiter;
		 NEWPMOS.push_back(GetIntVal(str));			    
	}// end of for loop
//...................................TESTING PURPOSE ONLY..FOR PMOS INT VECTOR.................................
	cout << "YOUR PMOS INT ARRAY IS : " << NEWPMOS.size() << endl;
	for (NEWPMOSiter = NEWPMOS.begin(); NEWPMOSiter < NEWPMOS.end(); NEWPMOSiter++)
	{
		cout <<*NEWPMOSiter<< endl; 
	}
//...................CONVERTING NMOS..........................................................................
					
    vector<string>::iterator NMOSiter;
	for ( NMOSiter = NMOS.begin(); NMOSiter < NMOS.end(); NMOSiter++ )
	{	
		 string str = *NMOSiter;
		 NEWNMOS.push_back(GetIntVal(str));
	}// end of for loop
//...................................TESTING PURPOSE ONLY.....................................
	cout << "YOUR NMOS INT ARRAY IS : " << NEWNMOS.size() << endl;
	for (NEWNMOSiter = NEWNMOS.begin(); NEWNMOSiter < NEWNMOS.end(); NEWNMOSiter++)
	{
		cout <<*NEWNMOSiter<< endl; 
	}
//............................................CAPACITORS CONVERT .................................................
	 vector<string>::iterator CAPACITORiter;
	for ( CAPACITORiter = CAPACITOR.begin(); CAPACITORiter < CAPACITOR.end(); CAPACITORiter++ )
	{	
		 string str = *CAPACITORiter;
		 NEWCAPACITOR.push_back(GetIntVal(str));
	}// end of for loop
//...................................TESTING PURPOSE ONLY.....................................
	cout << "YOUR CAPACITOR INT ARRAY IS : " << NEWCAPACITOR.size() << endl;
	for (NEWCAPACITORiter = NEWCAPACITOR.begin(); NEWCAPACITORiter < NEWCAPACITOR.end(); NEWCAPACITORiter++)
	{
		cout <<*NEWCAPACITORiter<< endl; 
	}
//................................................INDUCTOR CONVERT.........................................
	 vector<string>::iterator INDUCTORiter;
	for ( INDUCTORiter = INDUCTOR.begin(); INDUCTORiter < INDUCTOR.end(); INDUCTORiter++ )
	{	
		 string str = *INDUCTORiter;
		 NEWINDUCTOR.push_back(GetIntVal(str));
	}// end of for loop
//...................................TESTING PURPOSE ONLY.....................................
	cout << "YOUR INDUCTOR INT ARRAY IS : " << NEWINDUCTOR.size() << endl;
	for (NEWINDUCTORiter = NEWINDUCTOR.begin(); NEWINDUCTORiter < NEWINDUCTOR.end(); NEWINDUCTORiter++)
	{
		cout <<*NEWINDUCTORiter<< endl; 
	}
//............................................RESISTOR CONVERT..............................................
	 vector<string>::iterator RESISTORiter;
	for ( RESISTORiter = RESISTOR.begin(); RESISTORiter < RESISTOR.end(); RESISTORiter++ )
	{	
		 string str = *RESISTORiter;
		 NEWRESISTOR.push_back(GetIntVal(str));
	}// end of for loop
//...................................TESTING PURPOSE ONLY.....................................
	cout << "YOUR RESISTOR INT ARRAY IS : " << NEWRESISTOR.size() << endl;
	for (NEWRESISTORiter = NEWRESISTOR.begin(); NEWRESISTORiter < NEWRESISTOR.end(); NEWRESISTORiter++)
	{
		cout <<*NEWRESISTORiter << endl; 
	}
//..........................................VOLTAGE CONVERT.....................................................
	 vector<string>::iterator VOLTAGEiter;
	for ( VOLTAGEiter = VOLTAGE.begin(); VOLTAGEiter < VOLTAGE.end(); VOLTAGEiter++ )
	{	
		 string str = *VOLTAGEiter;
		 NEWVOLTAGE.push_back(GetIntVal(str));
	}// end of for loop
//...................................TESTING PURPOSE ONLY....................................
	cout << "YOUR VOLTAGE INT ARRAY IS : " << NEWVOLTAGE.size() << endl;
	for (NEWVOLTAGEiter = NEWVOLTAGE.begin(); NEWVOLTAGEiter < NEWVOLTAGE.end(); NEWVOLTAGEiter++)
	{
		cout <<*NEWVOLTAGEiter << endl; 
	}
 }// end of VectorConv
//..............................................................................................
int max1(double node1, int maxval)
{
      int node = int (node1);
	  if (maxval>node)
		  return maxval;
	  else return node;
	//int maxnode1 = reinterpret_cast<<int>>(<node1>);
}// end of max


/********************************************************************************************************************************************************/

/*
extern "C" {
	#include "f2clibs/f2c.h"
	#include "clapack.h"

	
}
*/


// Printing the matrices to the console//
void matrix_printf(gsl_matrix * A ){//needs iostream
	//this functin only prints 2 significant digits

	int row = A->size1;	
	int col = A->size2;

	for(int i = 0 ; i != row ; i++){
		for(int j = 0 ; j != col ; j++){
			printf("%.2g",gsl_matrix_get(A,i,j));//prints 2 significant digits
			if (j != col - 1)
			printf("\t");
			else
				printf("\n");
		}
	}
	}
	
// Printing the matrices to the console//	
void matrix2file(gsl_matrix * A ,const char fileName[]) {//takes a matrix and a string an input
	int row = A->size1;
	int col = A->size2;
	std::ofstream file;//declares an ofstream
	file.open (fileName);//"creates" a file named fileName

	for(int i = 0 ; i != row ; i++){
		for(int j = 0 ; j != col ; j++){
			file << gsl_matrix_get(A,i,j);	
			if (j != col - 1)
				file << " \t";
			else
				file << " \n";
		}
	}

	file.close();//closes file
}

///Solving Linear Matrix Equations Using LU Decomposition//

void matrix_solve(gsl_matrix * A , gsl_matrix * B, gsl_matrix * X){
	//performs the following operation: X = A\B
	//where \ is the gausian elimination operator
	int size = A->size1;
	int col = B->size2;
	gsl_vector_view b;
	gsl_vector_view x;
	int s;     
	gsl_permutation * p = gsl_permutation_alloc (size);
	gsl_linalg_LU_decomp (A, p, &s);

	for(int i = 0 ; i != col ; i++){
		b = gsl_matrix_column(B, i);		
		x = gsl_matrix_column(X,i);
 		gsl_linalg_LU_solve (A, p, &b.vector, &x.vector);
	}

	gsl_permutation_free (p);
}


gsl_matrix* filetoequation()
{	
	createMosArray (); // This function creates two types of vectors:  those containing strings and those containing doubles
    VectorConv();  // This function converts strings to doubles 
    //max();
	for(int i = resistor, j = 0 ; i!= nmos + resistor ; i++ , j++)
	{
	//remeber to adjust resistor row for already existing resistors
		double val = getval(NEWNMOS,NEWNMOSiter,8,j,0)+resistor;
		NEWRESISTOR.push_back(val);
	 // setval(NEWRESISTOR,NEWRESISTORiter,5,i,0,val);

	   val = getval(NEWNMOS,NEWNMOSiter,8,j,2);
	   NEWRESISTOR.push_back(val);
	  //setval(NEWRESISTOR,NEWRESISTORiter,5,i,1,val);

	   val = getval(NEWNMOS,NEWNMOSiter,8,j,3);
	   NEWRESISTOR.push_back(val);
	  //setval(NEWRESISTOR,NEWRESISTORiter,5,i,2,val);

	  // val = getval(NEWNMOS,NEWNMOSiter,8,j,2);
	    val = 1.5;
		NEWRESISTOR.push_back(val);

		val = 0; 
		NEWRESISTOR.push_back(val);
	 // setval(NEWRESISTOR,NEWRESISTORiter,5,i,3,1.5);
	 
	 cout << "YOUR RESISTORrrrrrrrrrrr INT ARRAY IS : " << NEWRESISTOR.size() << endl;
	for (NEWRESISTORiter = NEWRESISTOR.begin(); NEWRESISTORiter < NEWRESISTOR.end(); NEWRESISTORiter++)
	{
		cout <<*NEWRESISTORiter << endl; 
	}

	}//end of resistor for loop
	
	for(int i = resistor + nmos, j = 0  ; i!= nmos + resistor + pmos ; i++, j++)
	{
		double val = getval(NEWPMOS,NEWPMOSiter,8,j,0)+resistor+nmos;
		NEWRESISTOR.push_back(val);

		val = getval(NEWPMOS,NEWPMOSiter,8,j,2);
	   NEWRESISTOR.push_back(val);
	  //setval(NEWRESISTOR,NEWRESISTORiter,5,i,1,val);

	   val = getval(NEWPMOS,NEWPMOSiter,8,j,3);
	   NEWRESISTOR.push_back(val);
	  //setval(NEWRESISTOR,NEWRESISTORiter,5,i,2,val);

	  // val = getval(NEWNMOS,NEWNMOSiter,8,j,2);
	    val = 1.5;
		NEWRESISTOR.push_back(val);
		val = 0; 
		NEWRESISTOR.push_back(val);
	 // setval(NEWRESISTOR,NEWRESISTORiter,5,i,3,1.5);

		cout << "YOUR RESISTORrrrrrrrrrrr INT ARRAY IS : " << NEWRESISTOR.size() << endl;
		
	for (NEWRESISTORiter = NEWRESISTOR.begin(); NEWRESISTORiter < NEWRESISTOR.end(); NEWRESISTORiter++)
	{
		cout <<*NEWRESISTORiter << endl; 
	}
	}//end of for loop second time
	//nmos capacitors first and pmos after
	for(int i = capacitor , j = 0; i!= nmos + capacitor ; i++, j++)//capacitors for nmos
	{

		double val = getval(NEWNMOS,NEWNMOSiter,8,j,0)+ capacitor;
		NEWCAPACITOR.push_back(val);

		val = getval(NEWNMOS,NEWNMOSiter,8,j,1);
	   NEWCAPACITOR.push_back(val);
	  //setval(NEWRESISTOR,NEWRESISTORiter,5,i,1,val);

	   val = getval(NEWNMOS,NEWNMOSiter,8,j,3);
	   NEWCAPACITOR.push_back(val);
	  //setval(NEWRESISTOR,NEWRESISTORiter,5,i,2,val);

	  // val = getval(NEWNMOS,NEWNMOSiter,8,j,2);
	    val = 1.5;
		NEWCAPACITOR.push_back(val);

		val = 0; 
		NEWCAPACITOR.push_back(val);

		cout << "YOUR CAPACITORrrrrrrrrrrr INT ARRAY IS : " << NEWCAPACITOR.size() << endl;
	
	
	for (NEWCAPACITORiter = NEWCAPACITOR.begin(); NEWCAPACITORiter < NEWCAPACITOR.end(); NEWCAPACITORiter++)
	{
		cout <<*NEWCAPACITORiter << endl; 
	}
	
	} // END OF 3RD FOR LOOP....

	
	for(int i = capacitor + nmos , j = 0 ; i!= nmos + capacitor + pmos; i++ , j++)
	{
		double val = getval(NEWPMOS,NEWPMOSiter,8,j,0)+capacitor+nmos;
		NEWCAPACITOR.push_back(val);

		val = getval(NEWPMOS,NEWPMOSiter,8,j,1);
	   NEWCAPACITOR.push_back(val);
	  //setval(NEWRESISTOR,NEWRESISTORiter,5,i,1,val);

	   val = getval(NEWPMOS,NEWPMOSiter,8,j,3);
	   NEWCAPACITOR.push_back(val);
	  //setval(NEWRESISTOR,NEWRESISTORiter,5,i,2,val);

	  // val = getval(NEWNMOS,NEWNMOSiter,8,j,2);
	    val = 1.5;
		NEWCAPACITOR.push_back(val);
		val = 0; 
		NEWCAPACITOR.push_back(val);

		//
		cout << "YOUR CAPACITORrrrrrrrrrrr INT ARRAY IS : " << NEWCAPACITOR.size() << endl;
	for (NEWCAPACITORiter = NEWCAPACITOR.begin(); NEWCAPACITORiter < NEWCAPACITOR.end(); NEWCAPACITORiter++)
	{
		cout <<*NEWCAPACITORiter << endl; 
	}
	
	} // END OF 4TH FOR LOOP... 
	
	//nmos capacitors first and pmos after
	for(int i = 0 , j = 0; i!= nmos ; i++ , j++)//populate current cources nmos
	{
		
		double val = getval(NEWNMOS,NEWNMOSiter,8,j,0);
		NEWIVCONTROL.push_back(val);

		val = getval(NEWNMOS,NEWNMOSiter,8,j,3);
	   NEWIVCONTROL.push_back(val);
	  //setval(NEWRESISTOR,NEWRESISTORiter,5,i,1,val);

	   val = getval(NEWNMOS,NEWNMOSiter,8,j,1);
	   NEWIVCONTROL.push_back(val);
	  //setval(NEWRESISTOR,NEWRESISTORiter,5,i,2,val);

	   val = getval(NEWNMOS,NEWNMOSiter,8,j,2);
	   NEWIVCONTROL.push_back(val);

	   val = getval(NEWNMOS,NEWNMOSiter,8,j,3);
	   NEWIVCONTROL.push_back(val);

	  // val = getval(NEWNMOS,NEWNMOSiter,8,j,2);
	    val = 1.5;
		NEWIVCONTROL.push_back(val);
		
	cout << "YOUR IVCONTROLrrrrrrrrrrr INT ARRAY IS : " << NEWIVCONTROL.size() << endl;
	for (NEWIVCONTROLiter = NEWIVCONTROL.begin(); NEWIVCONTROLiter < NEWIVCONTROL.end(); NEWIVCONTROLiter++)
	{
		cout <<*NEWIVCONTROLiter << endl; 
	}

	} // END OF 5TH FOR LOOP 
	
	for(int i = nmos , j = 0; i!= nmos + pmos; i++ , j++ ) //populates caps for pmos
	{
		double val = getval(NEWPMOS,NEWPMOSiter,8,j,0)+nmos;
		NEWIVCONTROL.push_back(val);

		val = getval(NEWPMOS,NEWPMOSiter,8,j,1);
	   NEWIVCONTROL.push_back(val);
	  //setval(NEWRESISTOR,NEWRESISTORiter,5,i,1,val);

	   val = getval(NEWPMOS,NEWPMOSiter,8,j,3);
	   NEWIVCONTROL.push_back(val);
	  //setval(NEWRESISTOR,NEWRESISTORiter,5,i,2,val);

	   val = getval(NEWPMOS,NEWPMOSiter,8,j,2);
	   NEWIVCONTROL.push_back(val);

	   val = getval(NEWPMOS,NEWPMOSiter,8,j,3);
	   NEWIVCONTROL.push_back(val);

	  // val = getval(NEWNMOS,NEWNMOSiter,8,j,2);
	    val = 1.5;
		NEWIVCONTROL.push_back(val);

		cout << "YOUR IVCONTROLrrrrrrrrrrr INT ARRAY IS : " << NEWIVCONTROL.size() << endl;
	for (NEWIVCONTROLiter = NEWIVCONTROL.begin(); NEWIVCONTROLiter < NEWIVCONTROL.end(); NEWIVCONTROLiter++)
	{
		cout <<*NEWIVCONTROLiter << endl; 
	}
	//remmeber to adjust resistor row for already existing resistors
	} // end of cap pmos for loop.
	
	

	int maxval=0;
for (int i=0; i<pmos; i++) {
		double firstnode = getval(NEWPMOS,NEWPMOSiter,8,i,1);
		double secondnode = getval(NEWPMOS,NEWPMOSiter,8,i,2);
		maxval = max(firstnode,maxval);
		maxval = max(secondnode,maxval);
		
		}
		
	for (int i=0; i<nmos; i++) {
		double firstnode = getval(NEWNMOS,NEWNMOSiter,8,i,1);
		double secondnode = getval(NEWNMOS,NEWNMOSiter,8,i,2);
		maxval = max(firstnode,maxval);
		maxval = max(secondnode,maxval);
		}

	for (int i=0; i<capacitor; i++) {
		double firstnode = getval(NEWCAPACITOR,NEWCAPACITORiter,5,i,1);
		double secondnode = getval(NEWCAPACITOR,NEWCAPACITORiter,5,i,2);
		maxval = max(firstnode,maxval);
		maxval = max(secondnode,maxval);
		}

	for (int i=0; i<resistor; i++) {
		double firstnode = getval(NEWRESISTOR,NEWRESISTORiter,5,i,1);
		double secondnode = getval(NEWRESISTOR,NEWRESISTORiter,5,i,2);
		maxval = max(firstnode,maxval);
		maxval = max(secondnode,maxval);
		}

	for (int i=0; i<inductor; i++) {
		double firstnode = getval(NEWINDUCTOR,NEWINDUCTORiter,5,i,1);
		double secondnode = getval(NEWINDUCTOR,NEWINDUCTORiter,5,i,2);
		maxval = max(firstnode,maxval);
		maxval = max(secondnode,maxval);
		}


	for (int i=0; i<voltage; i++) {
		double firstnode = getval(NEWVOLTAGE,NEWVOLTAGEiter,6,i,1);
		double secondnode = getval(NEWVOLTAGE,NEWVOLTAGEiter,6,i,2);
		maxval = max(firstnode,maxval);
		maxval = max(secondnode,maxval);
		}	
		

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    int n = maxval;
	int l = inductor;
	int c = capacitor + nmos + pmos;
	int vs = voltage;
	int is = 0;
	int r = resistor + nmos + pmos;
	int ig = nmos + pmos;

//**************************build matrix equations**************************************//
// number of auxciliray variables
	int	aux = vs + is;
	// number of energy storage components
	int nec = l + c + vs + is;
	// number of energy storage variables
	int nev = nec + aux;
	// number of components
	int nc = nec + ig + r;
	// number of algebraic variables
	int nav = nc + ig + r + n;

	//algebraic differential equation matrix #1 (ADEM1)
	//holds coefficients for [vl; ic] <---- as show in sitax v3 file
	gsl_matrix * ADEM1 = gsl_matrix_calloc((l+c),(l+c));
	//mat ADEM1 = zeros<mat>((l+c),(l+c));
	
	
	//algegraic differential equation matrix #2 (ADEM2)
	//holds coefficients for [vl;ic;vis;ivs;vr;ir;vn]
	//is a square matrix
	gsl_matrix * ADEM2 = gsl_matrix_calloc((nav),(nav));
	//mat ADEM2 = zeros<mat>(nav,nav);
	
	//algebraic differential equation matrix # 3 (ADEM3)
	//holds coefficients for [il;vc;vs;is]
	gsl_matrix * ADEM3 = gsl_matrix_calloc((nav),(nec));
	//mat ADEM3 = zeros<mat>(nav,nec);

	//ordinary differential equation matrix
	//holds coefficients for [il;vc;vs;is;vss;iss]
  ODEM = gsl_matrix_calloc((nev),(nev));
	//mat ODEM = zeros<mat>(nev,nev);
	
	x0 = gsl_vector_alloc(nev);
	//x0 = gsl_matrix_calloc((1),(nev));
	//mat x0 = zeros<mat>(1, nev);
	
	//*****Capacitor********////
	for (int i = 0; i != c; i++){
	//dv = i/C
	
	
		int index = int (getval(NEWCAPACITOR,NEWCAPACITORiter,5,i,0))-1;
		double scientific = pow(10,getval (NEWCAPACITOR,NEWCAPACITORiter,5,i,4)); 
		double capacitance = getval (NEWCAPACITOR,NEWCAPACITORiter,5,i,3)*scientific; 
		int nA = int (getval(NEWCAPACITOR,NEWCAPACITORiter,5,i,1))-1;
		int nB = int (getval(NEWCAPACITOR,NEWCAPACITORiter,5,i,2))-1;
		int row = l+index;
	
	gsl_matrix_set(ADEM1, l + index, l + index, 1/capacitance);
	//ADEM1(l + index , l + index) = 1/capacitance;

 
	//Va and Vb location in branch equation
	//example: if Va > Vb then Va - Vb = Vc
	
	row = index + l; //l is the offset
	
	if (nA > nB){
	gsl_matrix_set(ADEM2, row, nc + r + nA, 1);
	//ADEM2(row , nc + r + nA ) = 1;
		if (nB != -1)
		gsl_matrix_set(ADEM2, row, nc + r + nB, -1);
		}
		//ADEM2(row , nc + r + nB ) = -1;}
	else{ 
		if (nA != -1)
		gsl_matrix_set(ADEM2, row, nc + r + nA, -1);
			//ADEM2(row , nc + r + nA ) = -1;
	gsl_matrix_set(ADEM2, row, nc + r + nB, 1);
	//ADEM2(row , nc + r + nB  ) = 1;
	}
//end of if statement

	//vC location in branch equation
	//ADEM3 will be on the RH side of ADEM2
	//ADEM3 holds coeff. for the energy storage variable
	
	gsl_matrix_set(ADEM3, l + index, l + index, 1);
		//ADEM3(l + index, l + index) = 1;
	
	//Ic location in the KCL equation
	if (nA > nB)//the current is leaving the node
		gsl_matrix_set(ADEM2, nc + nA, l + index, 1);
		//ADEM2(nc + nA , l + index) = 1;
	else {
		if (nA != -1)//make sure it's not ground
		gsl_matrix_set(ADEM2, nc + nA, l + index, -1);
		}
		//	ADEM2(nc + nA , l + index) = -1;}
	if (nB > nA)//the current is entering the node
		gsl_matrix_set(ADEM2, nc + nB, l + index, 1);
		//ADEM2(nc + nB , l + index) = 1;
	else{
		if (nB != -1)//ground check
		gsl_matrix_set(ADEM2, nc + nB, l + index, -1);	
			//ADEM2(nc + nB , l + index) = -1;
	}
	//Initial conditions(leaving as 0 for now)
	//gsl_matrix_set(x0, 0, l + index, 0);
	gsl_vector_set(x0,l + index,0);
	//x0(0 , l + index) = 0;
		} //end of giant for loop for capacitors
///////************Resistors***********////////////
	
	for (int i = 0; i != r; i++){
		
		int index = int (getval(NEWRESISTOR,NEWRESISTORiter,5,i,0))-1;
		double scientific = pow(10,getval (NEWRESISTOR,NEWRESISTORiter,5,i,4)); 
		double resistance = getval (NEWRESISTOR,NEWRESISTORiter,5,i,3)*scientific; 

		int nA = int (getval(NEWRESISTOR,NEWRESISTORiter,5,i,1))-1;
	
		int nB= int (getval(NEWRESISTOR,NEWRESISTORiter,5,i,2))-1;

	//thoeretical equation
	//vr - ir*R = 0
	gsl_matrix_set(ADEM2, nc + n + index,  l + c + vs + is + index, 1);
	//ADEM2(nc + n + index , l + c + vs + is + index) = 1;//where vr goes
	
	gsl_matrix_set(ADEM2, nc + n + index,  nc + index, (-1*resistance));
	//ADEM2(nc + n + index , nc + index) = -1*resistance;
	//va and vb location and branch equation
	if (nA > nB){
	gsl_matrix_set(ADEM2, l + c + vs + is + index,  nc + r + nA, 1);
	//ADEM2(l + c + vs + is + index, nc + r + nA) = 1;
	if (nB != -1) 
	gsl_matrix_set(ADEM2, l + c + vs + is + index,  nc + r + nB, -1);
	//	ADEM2(l + c + vs + is + index, nc + r + nB) = -1;
	}
	else {
		if (nA != -1 )
		gsl_matrix_set(ADEM2, l + c + vs + is + index,  nc + r + nA, -1);
		// ADEM2 ( l + c + vs + is + index, nc + r + nA) = -1;
	gsl_matrix_set(ADEM2, l + c + vs + is + index,  nc + r + nB, 1);
	//ADEM2(l+ c + vs + is + index, nc + r + nB) = 1;
	}
	//location in branch e	uation
	//example: -vr + va - vb = 0
	gsl_matrix_set(ADEM2, l + c + vs + is + index,  l + c + vs + is + index, -1);
	//ADEM2(l + c + vs + is + index, l + c + vs + is + index) = -1;

	//kcl equation for ir
	//two equation one for node and one for node b, but we have to find sign
	if (nA > nB)//menas its leaving node a
	gsl_matrix_set(ADEM2, nc + nA,  nc + index, 1);
		//ADEM2(nc + nA, nc + index) = 1;
	else 
		if (nA != -1) gsl_matrix_set(ADEM2, nc + nA,  nc + index, -1);
		//ADEM2(nc + nA, nc + index) = -1;
	
	if (nB > nA) 
	gsl_matrix_set(ADEM2, nc + nB,  nc + index, 1);
		//ADEM2 (nc + nB , nc + index) = 1;
	else 
		if (nB != -1) gsl_matrix_set(ADEM2, nc + nB,  nc + index, -1);
			//ADEM2 (nc + nB, nc + index) =-1;
	}//end of resistor big loop		
	
	/////////////*******Independent Voltage Sources (AC and DC)*************////////// 	
	//Array for volt source is VS
	//length of volt source is vs(lower)
	for(int i = 0; i != vs; i++) 
	{
		int index = int (getval(NEWVOLTAGE,NEWVOLTAGEiter,7,i,0))-1;
		double scientific1 = pow(10,getval (NEWVOLTAGE,NEWVOLTAGEiter,7,i,4)); 
		double amplitude = getval (NEWVOLTAGE,NEWVOLTAGEiter,7,i,3)*scientific1; 
		double scientific2 = pow(10,getval (NEWVOLTAGE,NEWVOLTAGEiter,7,i,6)); 
		double frequency = getval (NEWVOLTAGE,NEWVOLTAGEiter,7,i,5)*scientific2; 
		int nA = int (getval(NEWVOLTAGE,NEWVOLTAGEiter,7,i,1))-1;
	
		int nB= int (getval(NEWVOLTAGE,NEWVOLTAGEiter,7,i,2))-1;

	//Theoretical equation
	//by default, voltage sources are cosine
	//BUT a sin in needed to compute the cosine
	//dvs = -w*vs_sin	
	//dvs_sign = w*vs
	gsl_matrix_set(ODEM, l + c + index,   l +c+ vs + is + index, -1*frequency *2*3.14159);
	//ODEM(l + c + index, l + vs + is + index)=-1*frequency *2*3.14159;
	gsl_matrix_set(ODEM, l + c + vs + is + index,   l + c + index, frequency *2*3.14159);
	//ODEM(l + c + vs + is + index, l + c + index)=frequency*2*3.14159;

	
	//va and vb location for branch equation
	if (nA > nB){
	gsl_matrix_set(ADEM2, l + c + index,   nc + r + nA, 1);
		//ADEM2(l + c + index, nc + r + nA) = 1;
		if (nB != -1)
		 gsl_matrix_set(ADEM2, l + c + index,   nc + r + nB, -1);
			//ADEM2(l + c + index, nc + r + nB) = -1;
	}
	else {
		if (nA != -1 )
		gsl_matrix_set(ADEM2, l + c + index,   nc + r + nA, -1);
		 //ADEM2 ( l + c + index, nc + r + nA) = -1;
		 gsl_matrix_set(ADEM2, l + c + index,   nc + r + nB, 1);
	//ADEM2(l+ c + index, nc + r + nB) = 1;
	}

	
	//vs location in branch equation
	//because it is an energy storage variable
	//va - vb = vs
	gsl_matrix_set(ADEM3, l + c + index,   l + c + index, 1);
	//ADEM3 (l + c + index, l + c + index) = 1;

	//kcl
	if (nA > nB)//menas its leaving node a
	gsl_matrix_set(ADEM2, nc + nA,   l + c + index, 1);
		//ADEM2(nc + nA, l + c + index) = 1;
	else 
		if (nA != -1) gsl_matrix_set(ADEM2, nc + nA,   l + c + index, -1);
	//	ADEM2(nc + nA, l + c + index) = -1;
	
	if (nB > nA) 
	gsl_matrix_set(ADEM2, nc + nB , l + c + index, 1);
		//ADEM2 (nc + nB , l + c + index) = 1;
	else{ 
		if (nB != -1)
		gsl_matrix_set(ADEM2, nc + nB , l + c + index, -1);
			//ADEM2 (nc + nB , l + c + index) =-1;
	}
	
	if (nA > nB) gsl_vector_set(x0,l + c + index , -1*amplitude);
	//gsl_matrix_set(x0, 0 , l + c + index, -1*amplitude);
	//x0(0, l + c + index) = -1*amplitude;
	else  gsl_vector_set(x0,  l + c + index, amplitude);
	//gsl_matrix_set(x0, 0 , l + c + index, amplitude);
	//x0(0, l + c + index) = amplitude;
	}//end of giant vs for loop
		

/////////////********Dependent Current Sources***************///////////
	for(int i = 0; i != ig; i++)
	{

		int index = int (getval(NEWIVCONTROL,NEWIVCONTROLiter,6,i,0))-1;
		double gain = getval (NEWIVCONTROL,NEWIVCONTROLiter,6,i,5); 
		int nA = int (getval(NEWIVCONTROL,NEWIVCONTROLiter,6,i,1))-1;
		int nB= int (getval(NEWIVCONTROL,NEWIVCONTROLiter,6,i,2))-1;
		int nC= int (getval(NEWIVCONTROL,NEWIVCONTROLiter,6,i,3))-1;
		int nD= int (getval(NEWIVCONTROL,NEWIVCONTROLiter,6,i,4))-1;

gsl_matrix_set(ADEM2, nc + n + r + index, nc + n + r + index, 1);
	//ADEM2(nc + n + r + index, nc + n + r + index) = 1;
	if (nA != -1 ) 
		gsl_matrix_set(ADEM2, nc + n + r + index, nc + r + nA, gain);
		//ADEM2(nc + n + r + index, nc + r + nA) = gain;

	if (nB != -1)
		gsl_matrix_set(ADEM2, nc + n + r + index, nc + r + nB, (-1*gain));
		//ADEM2(nc + n + r + index, nc +r + nB) = -1*gain;


	//branch equation
	//vC and vD are the branch terminals
	//if vC > vD, vC - vD - vig = 0
	if (nC > nD){
	gsl_matrix_set(ADEM2, nec + r + index, nc + r + nC, 1);
		//ADEM2(nec + r + index, nc + r + nC) = 1;
	if (nD != -1) 
	gsl_matrix_set(ADEM2, nec + r + index, nc + r + nD, -1);
		//ADEM2(nec + r + index, nc + r + nD) = -1;
	}
	else{ 
		if (nC != -1 )
		gsl_matrix_set(ADEM2, nec + r + index, nc + r + nC, -1);
			//ADEM2(nec + r + index, nc + r + nC) = -1;
	gsl_matrix_set(ADEM2, nec + r + index, nc + r + nD, 1);		
	//ADEM2(nec + r + index, nc + r + nD) = 1;	
	}
	gsl_matrix_set(ADEM2, nec + r + index, nec + r + index, -1);
	//ADEM2(nec + r + index, nec + r + index) = -1;

	//KCL equations
	//current always leaves node c and thus will be positive
	//current always enters node d and will be negative
	if (nC != -1)
		gsl_matrix_set(ADEM2, nc + nC, nc + r + n + index, 1);
		//ADEM2(nc + nC, nc + r + n + index) = 1;

	
	if (nD != -1)
	gsl_matrix_set(ADEM2, nc + nD, nc + r + n + index, -1);
	//	ADEM2(nc + nD, nc + r + n + index) = -1;

	}//end of giant dep current source for loop
	
	//cout <<ADEM1<<endl;
	//cout <<ADEM2<<endl;
	//cout <<ADEM3<<endl;
	//cout <<x0<<endl;
	
	//gsl_matrix * ADEM1 = gsl_matrix_calloc((l+c),(l+c));
	//gsl_matrix * ADEM2 = gsl_matrix_calloc((nav),(nav));
	//gsl_matrix * ADEM3 = gsl_matrix_calloc((nav),(nec));
	//gsl_matrix * ODEM = gsl_matrix_calloc((nev),(nev));
	//gsl_matrix * x0 = gsl_matrix_calloc((1),(nev));
	for(int i = 0; i < (l+c); i++){
		for(int j = 0; j < (l + c) ; j++){
		cout<<gsl_matrix_get(ADEM1, i, j);
		cout<< " " ;
		if (j == (l + c)) cout <<endl;
		}}
	//gsl_matrix_fprint(stdout, ADEM1, %g);
	//mat tempM = solve(ADEM2, ADEM1);
	//mat ODEM1 = ADEM1 * tempM.submat(0,0, l + c -1, nec-1);
	//ODEM.submat(0,0, l + c -1, nec-1) = ODEM1; 
	
	// Printing the matrices for test purposes
		cout<<"This is ADEM1:"<<endl;
		matrix_printf(ADEM1);
		cout<<"This is ADEM2:"<<endl;
		matrix_printf(ADEM2);
		cout<<"This is ADEM3:"<<endl;
		matrix_printf(ADEM3);
		cout<<"This is x0:"<<endl;
		//matrix_printf(x0);
		cout<<"This is ODEM:"<<endl;
		matrix_printf(ODEM);
	
	
	//Allocating space for solution
	gsl_matrix * tempsolution = gsl_matrix_alloc((nav),(nec));
	
		
	//Solving the linear matrix equations
	
	matrix_solve(ADEM2,ADEM3,tempsolution); // The solution now is in tempsolution = ADEM2^-1*ADEM3.  
											// This is done using LU decompositioin and not the inverse operator
											// to save computation time and prevent rounding errors 
	
	gsl_matrix_view tempsolution2	// tempsolution2 is a portion of tempsolution 
					= gsl_matrix_submatrix (tempsolution,0,0,l+c,nec); 
				
											
	
		
	// subODEM is a portion of ODEM
	gsl_matrix_view subODEM = gsl_matrix_submatrix (ODEM,0,0,l+c,l+c+vs+is);
	
	//Multiply ADEM1 and tempsolution2 storing the result in subODEM
	// subODEM = ADEM1*tempsolution2
	gsl_blas_dgemm(CblasNoTrans,CblasNoTrans,1.0,ADEM1,&tempsolution2.matrix,0.0,&subODEM.matrix);
	
	
	// This clears memory allocated to tempsolution 
	gsl_matrix_free (tempsolution);
		
	// Print ODEM
	cout<<"This is ODEM:"<<endl;
	
	matrix_printf(ODEM);
	matrix2file(ODEM ,"ODEM.txt");
	
	
return ODEM;
		
}//end of "main" filetoequation
/*///////////////////////////////////////////////////////end of our code///////////////end//end//end//end//end//end////////////////*/
ZPLUGIN_BEGIN( sdp2010 );


void render() {



}

//int eToX( double x, double *y, double *dYdx, void *params ) {
//	dYdx[0] = y[0] * ((double *)params)[0];
//	return 0;
//}

int eToX (double x, double *y, double *dYdx, void *params) {


	gsl_matrix *params1 = (gsl_matrix*)params;
	int row = params1->size1;
	int column = params1->size2;
	for (int i=0; i<row; i++){
		for (int j=0; j<column; j++) {
			if (j==0) 
				dYdx[i]=0;
			dYdx[i]+=y[j]*gsl_matrix_get(params1,i,j);					
		}
	}
    cout<<dYdx[1]<< "   " << x <<endl;
    
    //solution << dYdx[0] << " ";
    /*
    gsl_vector *y1 = (gsl_vector*)y;
    for (int i=0; i<row; i++){
		for (int j=0; j<column; j++) {
			if (j==0) 
				dYdx[i]=0;
			dYdx[i]+=gsl_vector_get(y1,j)*gsl_matrix_get(params1,i,j);
			//double val = gsl_vector_get(y1,j);
			cout<<dYdx[i]<<endl;
			
		}
	}
    */
    
	return 0;
	}




void startup() {

	//opening file where the solution will be stored
	solution.open ("solution.txt");//"creates" a file named fileName


	gsl_matrix *params=filetoequation();
	int dims = params->size1;
	double initCond[] = {0.0, 2.0, 0.0};
	double startTime = 0.0;
	double endTime = 1.0;
	int storeOutput = 1;
	//double params[] = { -0.1 };
	//double *initCond = (double*)x0; 

	ZIntegratorGSLRK45 integator(
		dims, initCond, startTime, endTime, 1e-10, 1e-10, 1e-2, 1e-15, storeOutput, eToX, params
	);
	
	integator.integrate();
	
	//close file where the solution is stored
	solution.close();//closes file
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;


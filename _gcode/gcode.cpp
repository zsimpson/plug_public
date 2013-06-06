// @ZBS {
//		*MODULE_NAME Gcode
//		*MASTER_FILE 1
//		+DESCRIPTION {
//			Gcode tools
//		}
//		*PORTABILITY win32 unix macosx
//		*REQUIRED_FILES gcode.cpp gcode.h
//		*VERSION 1.0
// }


// OPERATING SYSTEM specific includes:
// SDK includes:
// STDLIB includes:
#include "math.h"
#include "stdio.h"
// MODULE includes:
// ZBSLIB includes:

#include "gcode.h"
#include "ztmpstr.h"
#include "tool.h"

float calcWid( float angle )
{
	// CALCULATE width of slot for angle
	float t = 1.f / 8.f; //thickness of wood
	float wid = t / tanf( angle / 2.f );
	return wid;
}

void toG( ZTLPVec<FVec3> toolPath, char* name )
{
    FILE *f = fopen( ZTmpStr("%s.nc", name), "wt" );
	fprintf( f, "%% \n" );
	fprintf( f, "g20 g01 f12 x0 y0 z0\n" );
	for( int i=0; i < toolPath.count; i++ ) 
	{
		fprintf( f, "x%f y%f z%f\n", toolPath[i]->x, toolPath[i]->y, toolPath[i]->z );
	}
	fprintf( f, "%% \n");
	fclose( f );

}

void toG( ToolPath* toolPath, char* name )
{
    FILE *f = fopen( ZTmpStr("%s.nc", name), "wt" );
	fprintf( f, "%% \n" );
	fprintf( f, "g20 g01 f12 x0 y0 z0\n" );
	for( int i=0; i < toolPath->instructions.count; i++ ) 
	{
		fprintf( f, "x%f y%f z%f\n", toolPath->instructions[i]->pos.x, toolPath->instructions[i]->pos.y, toolPath->instructions[i]->pos.z );
	}
	fprintf( f, "%% \n");
	fclose( f );

}

// Cuttable
//------------------------------------------------------------------------------------

void Cuttable::toG() 
{
	float depth = -0.05f;
	float passes = 3;
	FILE *f = fopen( fileName, "wt" );
	fprintf( f, "%% \n" );
	fprintf( f, "g20 g01 f12 x0 y0 z0\n" );
	for( int p=1; p<=passes; p++ )
	{
		fprintf( f, "x%f y%f z%f\n", toolPath[0].x, toolPath[0].y, (depth * p) - depth );
		for( int i=0; i<toolPath.count; i++ ) 
		{
			fprintf( f, "x%f y%f z%f\n", toolPath[i].x, toolPath[i].y, depth * p );
		}
		fprintf( f, "x%f y%f z%f\n", toolPath[0].x, toolPath[0].y, depth * p );
	}
	fprintf( f, "%% \n");
	fclose( f );
}

void Cuttable::makeToolPath()
{
	//start computing tool path
	const float toolRadius = 1.f / 16; 

	int d;
	float sum = 0.f;
	for( d=0; d<outline.count; d++ ) {
		//SOLVE for d0
		FVec2 d0 = outline[(d + 1) %outline.count];
		d0.sub(outline[d]);

		//SOLVE for d1
		FVec2 d1 = outline[(d + 2) % outline.count];
		d1.sub(outline[(d + 1) % outline.count]);

		FVec3 _d0( d0 );
		FVec3 _d1( d1 );

		_d0.cross( _d1 );
		sum+=_d0.z;
	}

	float sign = sum > 0.f ? -1.f : 1.f;
	/* //Use this if winding breaks.
	if(Linesanneal_manwind)
	{
		if(Linesanneal_wind)
			sign = 1.f;
		else
			sign = -1.f;
	}
	*/
	for( d=0; d<outline.count; d++ ) 
	{
		//SOLVE for d0
		FVec2 d0 = outline[(d + 1) % outline.count];
		d0.sub(outline[d]);

		//SOLVE for d1
		FVec2 d1 = outline[(d + 2) % outline.count];
		d1.sub(outline[(d + 1) % outline.count]);

		//SOLVE for v0
		FVec2 v0 = d0;
		v0.perp();
		v0.normalize();
		v0.mul( sign * toolRadius );

		//SOLVE for v1
		FVec2 v1 = d1;
		v1.perp();
		v1.normalize();
		v1.mul( sign * toolRadius );

		//SOLVE for p0
		FVec2 p0 = outline[d];
		p0.add( v0 );

		//SOLVE for p1
		FVec2 p1 = outline[(d+1)%outline.count];
		p1.add( v1 );
		//COMPUTE where the two lines meet to find the next vert for the toolPath
		FMat2 m;
		m.m[0][0] = d0.x;
		m.m[0][1] = d0.y;
		m.m[1][0] = -d1.x;
		m.m[1][1] = -d1.y;

		FVec2 b = FVec2( -p0.x + p1.x, -p0.y + p1.y );

		if( m.inverse() ) {
			FVec2 x = m.mul( b );
			FVec2 point( p0.x + (x.x * d0.x), p0.y + (x.x * d0.y) );
			toolPath.add( point );
		}

	}

}


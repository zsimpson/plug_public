// @ZBS {
//		*MODULE_OWNER_NAME gcode
// }


#ifndef GCODE_H
#define GCODE_H

#include "ztl.h"
#include "zvec.h"
#include "tool.h"

float calcWid( float angle );
	// calculates width of slot for angle

struct Cuttable
{
	// A piece ready to be exported to G-code
	ZTLVec<FVec2> outline;
	ZTLVec<FVec2> toolPath;
	char* fileName;

	void toG();
	void makeToolPath();
};

void toG( ZTLPVec<FVec3> toolPath, char* name );

void toG( ToolPath* toolPath, char* name );


#endif


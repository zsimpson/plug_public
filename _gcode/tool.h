#include "zvec.h"
#include "ztl.h"
#include "../_gcode/geom.h"

#ifndef TOOL_H
#define TOOL_H

struct ToolInstruction {
	DVec3 pos;
	double speed;

	ToolInstruction();
	ToolInstruction( DVec3 pos, double speed );
	// ETC
};

struct ToolPath {
	ZTLPVec<ToolInstruction> instructions;

	ToolPath();
	//ToolPath( const ToolPath &that );

	void add( DVec3 pt );
};

struct MaterialInfo {
	// Describes the material , max cutting speed, depth, 
	// things needed so that ToolPathGenerators can
	// 
};

ToolPath* toolPathGen_GeomShape( GeomShape *shape );
ToolPath* toolPathGen_Excavate( GeomShape *shape );

bool inline LineIntersectLine( DVec2 v1, DVec2 v2, DVec2 v3, DVec2 v4 );


#endif



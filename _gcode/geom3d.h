#ifndef GEOM3D_H
#define GEOM3D_H

#include "ztl.h"
#include "zvec.h"

// Geometry
//------------------------------------------------------------------

struct TriVert {
	DVec3 v1;
	DVec3 v2;
	DVec3 v3;

	TriVert();
	TriVert( DVec3 p1, DVec3 p2, DVec3 p3 ); 
};

struct GeomSurf {
	ZTLVec<TriVert> tris;

	GeomSurf();
	GeomSurf( GeomSurf &that );
	GeomSurf( int count, TriVert *tris );

	int vertCount() {
		return tris.count;
	}

	void addTri( TriVert v );
	void addTri( int count, TriVert *tris );
};




#endif


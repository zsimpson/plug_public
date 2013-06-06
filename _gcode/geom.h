#ifndef GEOM_H
#define GEOM_H

#include "ztl.h"
#include "zvec.h"

// Geometry
//------------------------------------------------------------------

struct GeomPoly {
	ZTLVec<DVec3> verts;

	GeomPoly();
	GeomPoly( GeomPoly &that );
	GeomPoly( int count, DVec3 *verts );
	GeomPoly( int count, DVec2 *verts );
	GeomPoly( int count, FVec3 *verts );
	GeomPoly( int count, FVec2 *verts );

	int vertCount() {
		return verts.count;
	}

	void addVert( DVec3 v );
	void addVert( FVec3 v );
	void addVert( DVec2 v );
	void addVert( FVec2 v );
	void addVert( int count, DVec3 *verts );
	void addVert( int count, DVec2 *verts );
	void addVert( int count, FVec3 *verts );
	void addVert( int count, FVec2 *verts );
};

struct GeomShape {
	ZTLPVec<GeomPoly> polys;
	ZTLVec<int> holes;

	GeomShape();
	GeomShape( const GeomShape &that );
	GeomShape( GeomPoly *poly );

	int polyCount() {
		assert( holes.count == polys.count );
		return polys.count;
	}

	void clear();
	void copy( GeomShape *other );

	void addPoly( GeomPoly *poly, int hole=0 );
		// Copys the poly

	void opUnion( GeomShape *otherShape );
	void opSub( GeomShape *otherShape );

	bool isInside( DVec2 p );
	bool lineIntersect( DVec2 p1, DVec2 p2 );
};

#endif


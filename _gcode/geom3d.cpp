#include "geom3d.h"
#include "gpc.h"
#include "tool.h"

// GeomSurf
//------------------------------------------------------------------

GeomSurf::GeomSurf(  ) {
}

GeomSurf::GeomSurf( GeomSurf &that ) {
	tris.copy( that.tris );
}

GeomSurf::GeomSurf( int count, TriVert *tris ) {
	for( int i=0; i<count; i++ ) {
		this->tris.add( tris[i] );
	}
}


void GeomSurf::addTri( TriVert v ) {
	tris.add( v );
}


void GeomSurf::addTri( int count, TriVert *tris ) {
	for( int i=0; i<count; i++ ) {
		this->tris.add( tris[i] );
	}
}



TriVert::TriVert()
{

}

TriVert::TriVert( DVec3 p1, DVec3 p2, DVec3 p3 )
{
	v1 = p1;
	v2 = p2;
	v3 = v3;
} 



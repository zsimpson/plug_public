#include "geom.h"
#include "gpc.h"
#include "tool.h"

// GeomPoly
//------------------------------------------------------------------

GeomPoly::GeomPoly(  ) {
}

GeomPoly::GeomPoly( GeomPoly &that ) {
	verts.copy( that.verts );
}

GeomPoly::GeomPoly( int count, DVec3 *verts ) {
	for( int i=0; i<count; i++ ) {
		this->verts.add( verts[i] );
	}
}

GeomPoly::GeomPoly( int count, DVec2 *verts ) {
	for( int i=0; i<count; i++ ) {
		this->verts.add( DVec3(verts[i].x,verts[i].y,0.0) );
	}
}

GeomPoly::GeomPoly( int count, FVec3 *verts ) {
	for( int i=0; i<count; i++ ) {
		this->verts.add( DVec3( (double)verts[i].x,(double)verts[i].y,(double)verts[i].z ) );
	}
}

GeomPoly::GeomPoly( int count, FVec2 *verts ) {
	for( int i=0; i<count; i++ ) {
		this->verts.add( DVec3( (double)verts[i].x,(double)verts[i].y, 0.0 ) );
	}
}

void GeomPoly::addVert( DVec3 v ) {
	verts.add( v );
}

void GeomPoly::addVert( FVec3 v ) {
	verts.add( DVec3( (double)v.x, (double)v.y, (double)v.z ) );
}

void GeomPoly::addVert( DVec2 v ) {
	verts.add( DVec3( v.x, v.y, 0.0 ) );
}

void GeomPoly::addVert( FVec2 v ) {
	verts.add( DVec3( (double)v.x, (double)v.y, 0.0 ) );
}

void GeomPoly::addVert( int count, DVec3 *verts ) {
	for( int i=0; i<count; i++ ) {
		this->verts.add( verts[i] );
	}
}

void GeomPoly::addVert( int count, DVec2 *verts ) {
	for( int i=0; i<count; i++ ) {
		this->verts.add( DVec3( verts[i].x, verts[i].y, 0.0 )  );
	}
}

void GeomPoly::addVert( int count, FVec3 *verts ) {
	for( int i=0; i<count; i++ ) {
		this->verts.add( DVec3( (double)verts[i].x, (double)verts[i].y, (double)verts[i].z )  );
	}
}

void GeomPoly::addVert( int count, FVec2 *verts ) {
	for( int i=0; i<count; i++ ) {
		this->verts.add( DVec3( (double)verts[i].x, (double)verts[i].y, 0.0 )  );
	}
}

// GeomShape
//------------------------------------------------------------------

GeomShape::GeomShape() {
	// @TODO
}

GeomShape::GeomShape( const GeomShape &that ) {
	// @TODO is this right?
	this->holes = that.holes;
	this->polys = that.polys;
}

GeomShape::GeomShape( GeomPoly *poly ) {
	GeomPoly *p = new GeomPoly( *poly );
	polys.add( p );
	holes.add( 0 );
}

void GeomShape::addPoly( GeomPoly *poly, int hole ) {
	GeomPoly *p = new GeomPoly( *poly );
	polys.add( p );
	holes.add( hole );
	assert( holes.count == polys.count );
}

void GeomShape::clear() {
	polys.clear();
	holes.clear();
}

void GeomShape::copy( GeomShape *other ) {
	clear();
	for( int i=0; i<other->polyCount(); i++ ) {
		addPoly( other->polys[i], other->holes[i] );
	}
}


gpc_polygon *geomShapeToGpcPolygon( GeomShape *shape ) {
	gpc_polygon *gpcPoly = new gpc_polygon;
	memset( gpcPoly, 0, sizeof(gpc_polygon) );

	for( int i=0; i<shape->polyCount(); i++ ) {
		// MAKE a gpc contour from out polygon.  This requires a loop because
		// we have DVec3 and they want DVec2
		gpc_vertex_list contour;
		contour.num_vertices = shape->polys[i]->vertCount();
		
		contour.vertex = (gpc_vertex *)new double[ 2 * contour.num_vertices ];
		for( int j=0; j<contour.num_vertices; j++ ) {
			contour.vertex[j].x = shape->polys[i]->verts[j].x;
			contour.vertex[j].y = shape->polys[i]->verts[j].y;
		}
		
		gpc_add_contour( gpcPoly, &contour, shape->holes[i] );

		// We think that gpc_add_contour is copying the data so now we must free the buffer
		// that was used to convert into 2D

		delete contour.vertex;
	}

	return gpcPoly;
}

GeomShape *gpcPolygonToGeomShape( gpc_polygon *gpcPolygon ) {
	GeomShape *geomShape = new GeomShape;

	for( int i=0; i<gpcPolygon->num_contours; i++ ) {
		GeomPoly poly( gpcPolygon->contour[i].num_vertices, (DVec2*)gpcPolygon->contour[i].vertex );
		geomShape->addPoly( &poly, gpcPolygon->hole[i] );
	}

	return geomShape;
}

void GeomShape::opUnion( GeomShape *otherShape ) {
	// Use the GPC library

	// @TODO: Assert that all polys in this and other are at z==0 because
	// the gpc system supports only 2D

	// MAKE the gpc "polygon" structs which are roughly equivilent to our "GeomShape"

	// CONVERT our shape to GPC's "polygon"
	gpc_polygon *gpcA = geomShapeToGpcPolygon( this );
	gpc_polygon *gpcB = geomShapeToGpcPolygon( otherShape );
	gpc_polygon gpcC;
	gpc_polygon_clip( GPC_UNION, gpcA, gpcB, &gpcC );

	// CONVERT GPC's "polygon" to our shape
	GeomShape *resultShape = gpcPolygonToGeomShape( &gpcC );

	copy( resultShape );
	// *this = *resultShape;
	// @TODO: Step into this above line and see if it does the same thing as copy constructor

	gpc_free_polygon( gpcA );
	gpc_free_polygon( gpcB );
	gpc_free_polygon( &gpcC );
}

void GeomShape::opSub( GeomShape *otherShape ) {
	// Use the GPC library

	// @TODO: Assert that all polys in this and other are at z==0 because
	// the gpc system supports only 2D

	// MAKE the gpc "polygon" structs which are roughly equivilent to our "GeomShape"

	// CONVERT our shape to GPC's "polygon"
	gpc_polygon *gpcA = geomShapeToGpcPolygon( this );
	gpc_polygon *gpcB = geomShapeToGpcPolygon( otherShape );
	gpc_polygon gpcC;
	gpc_polygon_clip( GPC_DIFF, gpcA, gpcB, &gpcC );

	// CONVERT GPC's "polygon" to our shape
	GeomShape *resultShape = gpcPolygonToGeomShape( &gpcC );

	copy( resultShape );
	// *this = *resultShape;
	// @TODO: Step into this above line and see if it does the same thing as copy constructor

	gpc_free_polygon( gpcA );
	gpc_free_polygon( gpcB );
	gpc_free_polygon( &gpcC );
}

bool GeomShape::isInside( DVec2 p ) {
	int numCrosses = 0;
	DVec2 startp = DVec2( p.x, 1000.0 ); 
	for( int nPoly = 0; nPoly < polys.count; nPoly = nPoly + 1 )
	{
		GeomPoly* outline = polys[nPoly];
		int d;
		double sum = 0.0;
		for( d=0; d<outline->vertCount(); d++ ) 
		{
			DVec2 l1 = DVec2( outline->verts[d % outline->vertCount()].x, outline->verts[d % outline->vertCount()].y );
			DVec2 l2 = DVec2( outline->verts[(d+1) % outline->vertCount()].x, outline->verts[(d+1) % outline->vertCount()].y );
			if( LineIntersectLine( p, startp, l1, l2 ) )
			{
				numCrosses++;
			}
			
		}
	}

	if(numCrosses % 2 == 1)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool GeomShape::lineIntersect( DVec2 p1, DVec2 p2 )
{
	bool intersect = false;
	for( int nPoly = 0; nPoly < polys.count; nPoly = nPoly + 1 )
	{
		GeomPoly* outline = polys[nPoly];
		int d;
		double sum = 0.0;
		for( d=0; d<outline->vertCount(); d++ ) 
		{
			DVec2 l1 = DVec2( outline->verts[d % outline->vertCount()].x, outline->verts[d % outline->vertCount()].y );
			DVec2 l2 = DVec2( outline->verts[(d+1) % outline->vertCount()].x, outline->verts[(d+1) % outline->vertCount()].y );
			if( LineIntersectLine( p1, p2, l1, l2 ) )
			{
				intersect = true;
			}
			
		}
	}
	return intersect;
}



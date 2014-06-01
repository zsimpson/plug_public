// OPERATING SYSTEM specific includes :
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "memory.h"
#include "string.h"
#include "math.h"
#include "float.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "ztmpstr.h"
#include "zviewpoint.h"
#include "zrand.h"
#include "ztime.h"
#include "zgltools.h"
#include "zmathtools.h"

ZPLUGIN_BEGIN( celledge );

ZVAR( float, Celledge_springForce, 2e-1 );
ZVAR( float, Celledge_dt, 0.5 );
ZVAR( float, Celledge_breakLen, 0.3 );
ZVAR( float, Celledge_createLen, 0.2 );
ZVAR( int, Celledge_iterPerRender, 200 );
ZVAR( float, Celledge_shrink, 10 );
ZVAR( float, Celledge_b, 3.f );
ZVAR( float, Celledge_perimeter, 1.f );
ZVAR( float, Celledge_interior, 1.f );
ZVAR( float, Celledge_sticky, 4.5f );
ZVAR( float, Celledge_natLen, 0.f );


float radius = 3.f;

#define MAX_CELLS (128)
#define MAX_SPRINGS (1024)

struct Vert {
	FVec3 pos;
	FVec3 force;
};

struct Cell {
	int active;
	Vert verts[6];	
	
	FVec3 center() {
		FVec3 p;
		for( int i=0; i<6; i++ ) {
			p.add( verts[i].pos );
		}
		p.div( 6.f );
		return p;
	}
};

struct Spring {
	int active;
	Vert *verts[2];
	float natLen;
	float color[3];
	int breakable;
	int type;
		#define STICKY (0)
		#define PERIMETER (1)
		#define CROSS_INTERIOR (2)
		#define AROUND_INTERIOR (3)
	
	float mag2() {
		FVec3 p = verts[0]->pos;
		p.sub( verts[1]->pos );
		return p.mag2();
	}
};

Cell cells[MAX_CELLS];
Spring springs[MAX_SPRINGS];
Cell *dragging = NULL;
Spring *selectedSpring = NULL;
float adjusting = 0;
float adjustingStartLen = 0.f;

Spring *springAdd() {
	Spring *empty = NULL;
	for( int i=0; i<MAX_SPRINGS; i++ ) {
		if( ! springs[i].active ) {
			springs[i].active = 1;
			return &springs[i];
		}
	}
	return NULL;
}

Spring *springFind( Vert *v0, Vert *v1 ) {
	for( int i=0; i<MAX_SPRINGS; i++ ) {
		if( springs[i].active ) {
			if(
				   (springs[i].verts[0] == v0 && springs[i].verts[1] == v1)
				|| (springs[i].verts[0] == v1 && springs[i].verts[1] == v0)
			) {
				return &springs[i];
			}
		}
	}
	return NULL;
}

Spring *springFindClosest( FVec3 p ) {
	int minI = 0;
	float minD = 1e10f;
	for( int i=0; i<MAX_SPRINGS; i++ ) {
		if( springs[i].active ) {
			FVec3 c = springs[i].verts[1]->pos;
			c.sub( springs[i].verts[0]->pos );
			FVec3 _d = zmathPointToLineSegment( springs[i].verts[0]->pos, c, p );
			float d = _d.mag();
			if( d < minD ) {
				minD = d;
				minI = i;
			}
		}
	}
	return &springs[minI];
}
			
void springDelete( Spring *s ) {
	s->active = 0;
}

void physics() {
	float springForceByType[4] = { 
		Celledge_sticky,
		Celledge_perimeter,
		Celledge_interior,
		Celledge_interior
	};

	float b = Celledge_b;
	float m = (radius - b) / 1.f;

	for( int i=0; i<MAX_CELLS; i++ ) {
		for( int j=0; j<6; j++ ) {
			cells[i].verts[j].force.origin();
		}
	}
	
	if( dragging ) {
		FVec3 mouse = zviewpointProjMouseOnZ0Plane();
		FVec3 force = dragging->center();
		force.sub( mouse );
		force.mul( 0.1f );
		for( int j=0; j<6; j++ ) {
			dragging->verts[j].force.sub( force );
		}
	}

	for( int i=0; i<MAX_SPRINGS; i++ ) {
		Spring *s = &springs[i];
		if( s->active == 1 ) {
			Vert *v0 = s->verts[0];
			Vert *v1 = s->verts[1];
			FVec3 d = v0->pos;
			d.sub( v1->pos );
			float m = d.mag();
			if( m > 0.01f ) {
				d.div( m );
				FVec3 normalized = d;
				d.mul( Celledge_springForce * (m - s->natLen) );
				d.mul( springForceByType[s->type] );
				d.boundLen( 1.f );
				v0->force.sub( d );
				v1->force.add( d );

//				if( s->type == PERIMETER ) {
//					float vertical = 1.f - fabs( normalized.dot( FVec3::YAxis ) );
//					springs[i].natLen = m * vertical + b;
//				}
			}
		}
	}

	for( int i=0; i<MAX_CELLS; i++ ) {
		for( int j=0; j<6; j++ ) {
			Vert &v = cells[i].verts[j];
			v.force.mul( Celledge_dt );		
			v.pos.add( v.force );
		}
	}
}

void buildAndBreakSprings() {
	for( int i=0; i<10; i++ ) {
		Cell *c = &cells[ zrandI(0,MAX_CELLS) ];
		if( c->active ) {
			Vert *v = &c->verts[ zrandI(0,6) ];
			for( int j=0; j<MAX_CELLS; j++ ) {
				Cell *cj = &cells[j];
				if( c != cj && cj->active ) {
					for( int k=0; k<6; k++ ) {
						FVec3 p = cj->verts[k].pos;
						p.sub( v->pos );
						if( p.mag2() < Celledge_createLen ) {
							// SEE if there's a spring set for this already
							// This can be massively optimized by storing a
							// spring list for each vert
							Spring *s = springFind( v, &cj->verts[k] );
							if( !s ) {
								s = springAdd();
								if( s ) {
									s->verts[0] = v;
									s->verts[1] = &cj->verts[k];
									s->natLen = 0.f;
									s->color[0] = 0.f;
									s->color[1] = 0.f;
									s->color[2] = 1.f;
									s->breakable = 1;
									s->type = STICKY;
								}
							}
						}
					}
				}
			}
		}
	}
	
	for( int i=0; i<10; i++ ) {
		Spring *s = &springs[ zrandI(0,MAX_SPRINGS) ];
		if( s->active && s->breakable ) {
			if( s->mag2() > Celledge_breakLen ) {
				springDelete( s );
			}
		}
	}
}

FVec3 hexQRToXY( int q, int r ) {
	// hex to pixel
	return FVec3( 3.f * 1.73f * ((float)q+r/2.f), 3.f * 1.5f * (float)r, 0.f );
}

void render() {
	zviewpointSetupView();
	FVec3 mouse = zviewpointProjMouseOnZ0Plane();

	if( selectedSpring ) {
		selectedSpring->natLen = adjustingStartLen * expf(mouse.y - adjusting);
		Celledge_natLen = selectedSpring->natLen;
	}

	for( int i=0; i<Celledge_iterPerRender; i++ ) {
		buildAndBreakSprings();
		physics();
	}

	glPointSize( 2.f );	
	glBegin( GL_POINTS );
	for( int i=0; i<MAX_CELLS; i++ ) {
		if( cells[i].active ) {
			for( int j=0; j<6; j++ ) {
				Vert &v = cells[i].verts[j];
				glVertex2fv( (GLfloat *)&v.pos );
			}
		}
	}
	glEnd();

	glColor3ub( 0, 0, 255 );
	glBegin( GL_LINES );
	for( int i=0; i<MAX_SPRINGS; i++ ) {
		Spring &s = springs[i];
		if( s.active ) {
			glColor3fv( (GLfloat *)s.color );
			glVertex3fv( s.verts[0]->pos );
			glVertex3fv( s.verts[1]->pos );
		}
	}
	glEnd();

	if( selectedSpring ) {
		glColor3ub( 0, 0, 255 );
		glLineWidth( 3.f );
		glBegin( GL_LINES );
			glColor3fv( (GLfloat *)selectedSpring->color );
			glVertex3fv( selectedSpring->verts[0]->pos );
			glVertex3fv( selectedSpring->verts[1]->pos );
		glEnd();
	}
}

void reset() {
	memset( springs, 0, sizeof(springs) );
	memset( cells, 0, sizeof(cells) );
	
	int qn = 6;
	int rn = 6;

	int cellCount = 0;
	int springCount = 0;
	for( int q=0; q<qn; q++ ) {
		for( int r=0; r<rn; r++ ) {
			FVec3 center = hexQRToXY( q, r );

			// MAKE verts for cell
			cells[cellCount].active = 1;
			for( int a=0; a<6; a++ ) {
				float _a = (float)a / 6.f * 3.14f * 2.f + (30.f/360.f*2.f*3.14f);
				cells[cellCount].verts[a].pos = FVec3( radius * cosf(_a), radius * sinf(_a), 0.f );
				cells[cellCount].verts[a].pos.add( center );
			}
			
			// MAKE springs for cell perimeter
			for( int j=0; j<6; j++ ) {
				Spring *s = springAdd();
				s->verts[0] = &cells[cellCount].verts[(j+0)%6];
				s->verts[1] = &cells[cellCount].verts[(j+1)%6];
				s->natLen = radius;
				s->color[0] = 1.f;
				s->breakable = 0;
				s->type = PERIMETER;
			}
			
			// MAKE springs for cross cell interior
			for( int j=0; j<3; j++ ) {
				Spring *s = springAdd();
				s->verts[0] = &cells[cellCount].verts[(j+0)%6];
				s->verts[1] = &cells[cellCount].verts[(j+3)%6];
				s->natLen = 2.f*radius;
				s->breakable = 0;
				s->type = CROSS_INTERIOR;
			}
			
			// MAKE springs for cross perimeter interior
			for( int j=0; j<6; j++ ) {
				Spring *s = springAdd();
				s->verts[0] = &cells[cellCount].verts[(j+0)%6];
				s->verts[1] = &cells[cellCount].verts[(j+2)%6];
				s->natLen = 1.5f*radius;
				s->breakable = 0;
				s->type = AROUND_INTERIOR;
			}

			cellCount++;
		}
	}
}

void startup() {
   	reset();
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
	if( zMsgIsUsed() ) return;

	zviewpointHandleMsg( msg );
	zviewpointSetupView();
	FVec3 mouse = zviewpointProjMouseOnZ0Plane();

	if( zmsgIs(type,Key) && zmsgIs(which,q) ) {
		reset();
	}
	
	if( zmsgIs(type,ZUIMouseClickOn) && zmsgIs(dir,D) && zmsgIs(which,L) && zmsgIs(shift,1) ) {
		selectedSpring = springFindClosest( mouse );
		adjusting = mouse.y;																												cd 
		adjustingStartLen = selectedSpring->natLen;
	}
	else {
		if( zmsgIs(type,ZUIMouseClickOn) && zmsgIs(dir,D) && zmsgIs(which,L) ) {
			float closestD = 1e10f;
			for( int i=0; i<MAX_CELLS; i++ ) {
				if( cells[i].active ) {
					FVec3 cen = cells[i].center();
					cen.sub( mouse );
					float d = cen.mag2();
					if( d < closestD ) {
						closestD = d;
						dragging = &cells[i];
					}
				}
			}
		}
	}
	
	if( zmsgIs(type,ZUIMouseClickOn) && zmsgIs(dir,U) && zmsgIs(which,L) ) {
		selectedSpring = NULL;
		adjusting = 0.f;
		dragging = NULL;
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;


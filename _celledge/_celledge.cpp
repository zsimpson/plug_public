// OPERATING SYSTEM specific includes :
#ifdef __APPLE__
	#include "OpenGL/gl.h"
	#include "OpenGL/glu.h"
#else
	#include "wingl.h"
	#include "GL/gl.h"
	#include "GL/glu.h"
#endif
// SDK includes:
// STDLIB includes:
#include "memory.h"
#include "string.h"
#include "math.h"
#include "float.h"
#include "assert.h"
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
ZVAR( float, Celledge_dt, 0.05 );

ZVAR( float, Celledge_edgeSpring, 1 );
ZVAR( float, Celledge_edgeTorque, 1 );

ZVAR( float, Celledge_forceDraw, 300.0 );

ZVAR( float, Celledge_breakLen, 1.3 );
ZVAR( float, Celledge_createLen, 1 );

ZVAR( int, Celledge_iterPerRender, 1 );
ZVAR( int, Celledge_makeBreakCount, 10 );
ZVAR( float, Celledge_shrink, 10 );
ZVAR( float, Celledge_b, 3.f );
ZVAR( float, Celledge_perimeter, 1.f );
ZVAR( float, Celledge_interior, 1.f );
ZVAR( float, Celledge_sticky, 4.5f );
ZVAR( float, Celledge_natLen, 0.f );
ZVAR( float, Celledge_lenSpeed, 1e-3 );
ZVAR( float, Celledge_elongateP1, 1.f );
ZVAR( float, Celledge_elongateP2, 2.29f );
ZVAR( float, Celledge_elongateP3, 2.63f );
ZVAR( float, Celledge_elongateP4, 1.9f );




float radius = 3.f;

#define MAX_CELLS (128)
#define MAX_SPRINGS (8192)
#define MAX_EDGE_SPRINGS (512)
#define MAX_SIDES (64)

struct Vert {
	FVec3 pos;
	FVec3 force;
};

struct EdgeSpring {
	struct Cell *c0;
	struct Cell *c1;
	int v0;
	int v1;
	int active;

	float mag2();
	float orient();
	FVec3 com0();
	FVec3 com1();
};

struct Cell {
	int active;
	int numSides;
	int reproducing;
	int selected;
	int squished;
	Vert verts[MAX_SIDES];	
	EdgeSpring *edgeSprings[MAX_SIDES];
	
	FVec3 center() {
		FVec3 p;
		for( int i=0; i<numSides; i++ ) {
			p.add( verts[i].pos );
		}
		p.div( (float)numSides );
		return p;
	}

	void update() {
		if( reproducing ) {
			int v0 = 0;
			int v1 = numSides / 2;
			FVec3 a = verts[v0].pos;
			a.sub( verts[v1].pos );
			if( a.mag2() < 0.01f ) {
				// Create a new cell.  @TODO Hard
			}
		}
	}

	int inside( FVec3 test ) {
		int i, j, c = 0;
		for( i = 0, j = numSides-1; i < numSides; j = i++ ) {
			if( ((verts[i].pos.y > test.y) != (verts[j].pos.y>test.y)) && (test.x < (verts[j].pos.x-verts[i].pos.x) * (test.y-verts[i].pos.y) / (verts[j].pos.y-verts[i].pos.y) + verts[i].pos.x) ) {
				c = !c;
			}
		}
		return c;
	}

};

struct Spring {
	int active;
	Vert *verts[2];
	float origNatLen;
	float currNatLen;
	float targNatLen;

	float color[3];
	int breakable;
	Cell *cell;
	int type;
		#define STICKY (0)
		#define INTERIOR (1)
	
	float mag2() {
		FVec3 p = verts[0]->pos;
		p.sub( verts[1]->pos );
		return p.mag2();
	}
	
	FVec3 dir() {
		FVec3 a = verts[1]->pos;
		a.sub( verts[0]->pos );
		return a;
	}
};

float EdgeSpring::mag2() {
	FVec3 c0 = com0();
	FVec3 c1 = com1();
	FVec3 dist = c1;
	dist.sub( c0 );
	return dist.mag2();
}

float EdgeSpring::orient() {
	Vert *vert0 = &c0->verts[v0];
	Vert *vert1 = &c0->verts[ (v0+1) % c0->numSides ];
	FVec3 dir0 = vert1->pos;
	dir0.sub( vert0->pos );

	Vert *w0 = &c1->verts[v1];
	Vert *w1 = &c1->verts[ (v1+1) % c1->numSides ];
	FVec3 dir1 = w1->pos;
	dir1.sub( w0->pos );
	
	return dir0.dot( dir1 );
}

FVec3 EdgeSpring::com0() {
	Vert *vert0 = &c0->verts[v0];
	Vert *vert1 = &c0->verts[ (v0+1) % c0->numSides ];
	FVec3 com0 = vert1->pos;
	com0.sub( vert0->pos );
	com0.mul( 0.5f );
	com0.add( vert0->pos );
	return com0;
}

FVec3 EdgeSpring::com1() {
	Vert *w0 = &c1->verts[v1];
	Vert *w1 = &c1->verts[ (v1+1) % c1->numSides ];
	FVec3 com1 = w1->pos;
	com1.sub( w0->pos );
	com1.mul( 0.5f );
	com1.add( w0->pos );
	return com1;
}


Cell cells[MAX_CELLS];
Spring springs[MAX_SPRINGS];
EdgeSpring edgeSprings[MAX_EDGE_SPRINGS];
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
	assert( !"Too many springs" );
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

EdgeSpring *edgeSpringAdd() {
	EdgeSpring *empty = NULL;
	for( int i=0; i<MAX_EDGE_SPRINGS; i++ ) {
		if( ! edgeSprings[i].active ) {
			edgeSprings[i].active = 1;
			return &edgeSprings[i];
		}
	}
	return NULL;
}

void edgeSpringDelete( EdgeSpring *s ) {
	s->active = 0;
	s->c0->edgeSprings[s->v0] = NULL;
	s->c1->edgeSprings[s->v1] = NULL;
}


void cellReproduce( Cell *c ) {
	c->reproducing = 1;

	// PICK opposite verts and reduce the length of all their springs
	int v0 = 0;
	int v1 = c->numSides / 2;

	// FIND every interior spring that touches v0
	for( int i=0; i<MAX_SPRINGS; i++ ) {
		Spring *s = &springs[i];
		if( s->type == INTERIOR && (s->verts[0] == &c->verts[v0] || s->verts[1] == &c->verts[v0]) ) {
			s->currNatLen = 0.f;
		}
		if( s->type == INTERIOR && (s->verts[0] == &c->verts[v1] || s->verts[1] == &c->verts[v1]) ) {
			s->currNatLen = 0.f;
		}
	}
}

void cellSquish( Cell *c ) {
	// ADJUST every spring in proportion to its horizontal polarity
	c->squished = 1;
	for( int i=0; i<MAX_SPRINGS; i++ ) {
		if( springs[i].active && springs[i].cell == c ) {
			FVec3 a = springs[i].dir();
			if( a.mag2() > 0.001f ) {
				a.normalize();
				float dot = fabs( a.dot( FVec3::XAxis ) );

				// Linear
				//springs[i].natLen = springs[i].origNatLen * dot * Celledge_elongateP1 + Celledge_elongateP2;

				// Sigmoid
				springs[i].targNatLen = springs[i].origNatLen * Celledge_elongateP3 * zmathSigmoid( Celledge_elongateP2*(dot-Celledge_elongateP1) ) + Celledge_elongateP4;
			}
		}
	}
}

void cellNormalize( Cell *c ) {
	// RETURN every spring to its original length
	c->squished = 0;
	for( int i=0; i<MAX_SPRINGS; i++ ) {
		if( springs[i].active && springs[i].cell == c ) {
			springs[i].targNatLen = springs[i].origNatLen;
		}
	}
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
		for( int j=0; j<cells[i].numSides; j++ ) {
			cells[i].update();
			cells[i].verts[j].force.origin();
		}
	}
	
	if( dragging ) {
		FVec3 mouse = zviewpointProjMouseOnZ0Plane();
		FVec3 force = dragging->center();
		force.sub( mouse );
		force.mul( 0.1f );
		for( int j=0; j<dragging->numSides; j++ ) {
//		for( int j=0; j<1; j++ ) {
			dragging->verts[j].force.sub( force );
		}
	}

	for( int i=0; i<MAX_SPRINGS; i++ ) {
		Spring *s = &springs[i];
		if( s->active ) {
			s->currNatLen += Celledge_lenSpeed * (s->targNatLen - s->currNatLen);
		
			Vert *v0 = s->verts[0];
			Vert *v1 = s->verts[1];
			FVec3 d = v0->pos;
			d.sub( v1->pos );
			float m = d.mag();
			if( m > 0.01f ) {
				d.div( m );
				FVec3 normalized = d;
				d.mul( Celledge_springForce * (m - s->currNatLen) );
				d.mul( springForceByType[s->type] );
				d.boundLen( 1.f );
				v0->force.sub( d );
				v1->force.add( d );
			}
		}
	}

	for( int i=0; i<MAX_EDGE_SPRINGS; i++ ) {
		EdgeSpring *s = &edgeSprings[i];
		if( s->active ) {
			FVec3 d = s->com1();
			d.sub( s->com0() );
			d.mul( Celledge_edgeSpring );
			s->c0->verts[s->v0].force.add( d );
			s->c0->verts[(s->v0+1)%s->c0->numSides].force.add( d );
			d.mul( -1.f );
			s->c1->verts[s->v1].force.add( d );
			s->c1->verts[(s->v1+1)%s->c1->numSides].force.add( d );
			/*
			glColor3ub( 0, 255, 0 );
			zglDrawVector(
				0.f, 0.f, 0.f,
				s->c0->verts[s->v0].pos.x, s->c0->verts[s->v0].pos.y, s->c0->verts[s->v0].pos.z
			);
			
			glColor3ub( 255, 0, 0 );
			zglDrawVector(
				0.f, 0.f, 0.f,
				s->c1->verts[s->v1].pos.x, s->c1->verts[s->v1].pos.y, s->c1->verts[s->v1].pos.z
			);
			*/
			
			// Torque
			FVec3 a = s->c0->verts[s->v0].pos;
			a.sub( s->c0->verts[(s->v0+1)%s->c0->numSides].pos );
			a.normalize();

			FVec3 b = s->c1->verts[s->v1].pos;
			b.sub( s->c1->verts[(s->v1+1)%s->c1->numSides].pos );
			b.normalize();
			
			FVec3 ta0 = a;
			ta0.cross( b );
			float t0 = ta0.z;
			
			
			FVec3 a0tob1 = s->c1->verts[(s->v1+1)%s->c1->numSides].pos;
			a0tob1.sub( s->c0->verts[s->v0].pos );
			/*
			zglDrawVector(
				s->c0->verts[s->v0].pos.x, s->c0->verts[s->v0].pos.y, s->c0->verts[s->v0].pos.z,
				a0tob1.x, a0tob1.y, a0tob1.z
			);
			*/
			
			FVec3 a1tob0 = s->c1->verts[s->v1].pos;
			a1tob0.sub( s->c0->verts[(s->v0+1)%s->c0->numSides].pos );
			/*
			zglDrawVector(
				s->c0->verts[(s->v0+1)%s->c0->numSides].pos.x, s->c0->verts[(s->v0+1)%s->c0->numSides].pos.y, s->c0->verts[(s->v0+1)%s->c0->numSides].pos.z,
				a1tob0.x, a1tob0.y, a1tob0.z
			);
			*/
			

			FVec3 a0 = a;
			FVec2 a1( a0.x, a0.y );
			a1.perp();
			a0 = FVec3( a1.x, a1.y, 0.f );
			a0.mul( Celledge_edgeTorque * t0 );
			
			if( a0.dot(a0tob1) < 0.f ) {
				a0.mul( -1.f );
			}
			
			s->c0->verts[s->v0].force.add( a0 );
			s->c0->verts[(s->v0+1)%s->c0->numSides].force.sub( a0 );

			/*			
			zglDrawVector(
				s->c0->verts[s->v0].pos.x, s->c0->verts[s->v0].pos.y, s->c0->verts[s->v0].pos.z, 
				Celledge_forceDraw*a0.x, Celledge_forceDraw*a0.y, Celledge_forceDraw*a0.z
			);
			zglDrawVector(
				s->c0->verts[(s->v0+1)%s->c0->numSides].pos.x, s->c0->verts[(s->v0+1)%s->c0->numSides].pos.y, s->c0->verts[(s->v0+1)%s->c0->numSides].pos.z, 
				-Celledge_forceDraw*a0.x, -Celledge_forceDraw*a0.y, -Celledge_forceDraw*a0.z
			);
			*/

			FVec3 b0 = b;
			FVec2 b1( b0.x, b0.y );
			b1.perp();
			b0 = FVec3( b1.x, b1.y, 0.f );
			b0.mul( Celledge_edgeTorque * t0 );

			if( b0.dot(a1tob0) < 0.f ) {
				b0.mul( -1.f );
			}

			s->c1->verts[s->v1].force.sub( b0 );
			s->c1->verts[(s->v1+1)%s->c1->numSides].force.add( b0 );

			/*
			zglDrawVector(
				s->c1->verts[s->v1].pos.x, s->c1->verts[s->v1].pos.y, s->c1->verts[s->v1].pos.z, 
				-Celledge_forceDraw*b0.x, -Celledge_forceDraw*b0.y, -Celledge_forceDraw*b0.z
			);
			zglDrawVector(
				s->c1->verts[(s->v1+1)%s->c1->numSides].pos.x, s->c1->verts[(s->v1+1)%s->c1->numSides].pos.y, s->c1->verts[(s->v1+1)%s->c1->numSides].pos.z, 
				Celledge_forceDraw*b0.x, Celledge_forceDraw*b0.y, Celledge_forceDraw*b0.z
			);
			*/
		}
	}

	for( int i=0; i<MAX_CELLS; i++ ) {
		for( int j=0; j<cells[i].numSides; j++ ) {
			Vert &v = cells[i].verts[j];
			v.force.mul( Celledge_dt );		
			v.pos.add( v.force );
		}
	}
}

void buildAndBreakVertexSprings() {
	for( int i=0; i<Celledge_makeBreakCount; i++ ) {
		Cell *c = &cells[ zrandI(0,MAX_CELLS) ];
		if( c->active ) {
			Vert *v = &c->verts[ zrandI(0,c->numSides) ];
			for( int j=0; j<MAX_CELLS; j++ ) {
				Cell *cj = &cells[j];
				if( c != cj && cj->active ) {
					for( int k=0; k<cj->numSides; k++ ) {
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
									s->currNatLen = 0.f;
									s->targNatLen = 0.f;
									s->origNatLen = 0.f;
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
	
	for( int i=0; i<Celledge_makeBreakCount; i++ ) {
		Spring *s = &springs[ zrandI(0,MAX_SPRINGS) ];
		if( s->active && s->breakable ) {
			if( s->mag2() > Celledge_breakLen ) {
				springDelete( s );
			}
		}
	}
}

void buildAndBreakEdgeSprings() {
	for( int i=0; i<Celledge_makeBreakCount; i++ ) {
		Cell *ci = &cells[ zrandI(0,MAX_CELLS) ];
		if( ci->active ) {
			int _v0 = zrandI(0,ci->numSides);
			int _v1 = (_v0+1) % ci->numSides;
			
			if( ! ci->edgeSprings[_v0] ) {
			
				Vert *v0 = &ci->verts[_v0];
				Vert *v1 = &ci->verts[_v1];
				FVec3 comI = v1->pos;
				comI.sub( v0->pos );
				comI.mul( 0.5f );
				comI.add( v0->pos );
				
				for( int j=0; j<MAX_CELLS; j++ ) {
					Cell *cj = &cells[j];
					if( ci != cj && cj->active ) {
						for( int k=0; k<cj->numSides; k++ ) {
							if( ! cj->edgeSprings[k] ) {
								int _vk0 = zrandI(0,cj->numSides);
								int _vk1 = (_vk0+1) % cj->numSides;
								Vert *vk0 = &cj->verts[_vk0];
								Vert *vk1 = &cj->verts[_vk1];
								FVec3 comK = vk1->pos;
								comK.sub( vk0->pos );
								comK.mul( 0.5f );
								comK.add( vk0->pos );
								
								FVec3 dist = comK;
								dist.sub( comI );
								
								if( dist.mag2() < Celledge_createLen ) {
									EdgeSpring *edgeSpring = edgeSpringAdd();
									if( edgeSpring ) {
										edgeSpring->c0 = ci;
										edgeSpring->c1 = cj;
										edgeSpring->v0 = _v0;
										edgeSpring->v1 = _vk0;
										edgeSpring->active = 1;
										ci->edgeSprings[_v0]  = edgeSpring;
										cj->edgeSprings[_vk0] = edgeSpring;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	for( int i=0; i<Celledge_makeBreakCount; i++ ) {
		EdgeSpring *s = &edgeSprings[ zrandI(0,MAX_EDGE_SPRINGS) ];
		if( s->active ) {
			if( s->mag2() > Celledge_breakLen ) {
				edgeSpringDelete( s );
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
		selectedSpring->currNatLen = adjustingStartLen * expf(mouse.y - adjusting);
		Celledge_natLen = selectedSpring->currNatLen;
	}

	for( int i=0; i<Celledge_iterPerRender; i++ ) {
		//buildAndBreakVertexSprings();
		buildAndBreakEdgeSprings();
		physics();
	}

	glPointSize( 2.f );	
	for( int i=0; i<MAX_CELLS; i++ ) {
		if( cells[i].active && cells[i].selected ) {
			glBegin( GL_POLYGON );
			for( int j=0; j<cells[i].numSides; j++ ) {
				Vert &v = cells[i].verts[j];
				glVertex2fv( (GLfloat *)&v.pos );
			}
			glEnd();
		}
/*
		for( int j=0; j<cells[i].numSides; j++ ) {
			FVec3 force = cells[i].verts[j].force;
			force.mul( Celledge_forceDraw );
			glColor3ub( 0, 0, 255 );
			zglDrawVector(
				cells[i].verts[j].pos.x, cells[i].verts[j].pos.y, cells[i].verts[j].pos.z,
				force.x, force.y, force.z
			);
		}
*/
	}

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

	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glColor4ub( 128, 128, 255, 128 );
	for( int i=0; i<MAX_EDGE_SPRINGS; i++ ) {
		EdgeSpring &s = edgeSprings[i];
		if( s.active ) {
			/*
			if( s.orient() > 0.f ) {
				glBegin( GL_POLYGON );
					glVertex3fv( (GLfloat *)&s.c0->verts[s.v0].pos );
					glVertex3fv( (GLfloat *)&s.c0->verts[(s.v0+1)%s.c0->numSides].pos );
					glVertex3fv( (GLfloat *)&s.c1->verts[(s.v1+1)%s.c1->numSides].pos );
					glVertex3fv( (GLfloat *)&s.c1->verts[s.v1].pos );
				glEnd();
			}
			else {
			*/
				glBegin( GL_POLYGON );
					glVertex3fv( (GLfloat *)&s.c0->verts[s.v0].pos );
					glVertex3fv( (GLfloat *)&s.c0->verts[(s.v0+1)%s.c0->numSides].pos );
					glVertex3fv( (GLfloat *)&s.c1->verts[s.v1].pos );
					glVertex3fv( (GLfloat *)&s.c1->verts[(s.v1+1)%s.c1->numSides].pos );
				glEnd();
			//}
		}
	}
	glDisable( GL_BLEND );

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
	memset( edgeSprings, 0, sizeof(edgeSprings) );
	memset( springs, 0, sizeof(springs) );
	memset( cells, 0, sizeof(cells) );
	
	int qn = 4;
	int rn = 4;

	int cellCount = 0;
	int springCount = 0;
	for( int q=0; q<qn; q++ ) {
		for( int r=0; r<rn; r++ ) {
			FVec3 center = hexQRToXY( q, r );

			int sides = 12;

			// MAKE verts for cell
			cells[cellCount].active = 1;
			cells[cellCount].numSides = sides;
			for( int a=0; a<sides; a++ ) {
				float _a = (float)a / (float)sides * 3.14f * 2.f + (30.f/360.f*2.f*3.14f);
				cells[cellCount].verts[a].pos = FVec3( radius * cosf(_a), radius * sinf(_a), 0.f );
				cells[cellCount].verts[a].pos.add( center );
			}

			// MAKE interior springs
			for( int j=0; j<sides; j++ ) {
				for( int k=j+1; k<sides; k++ ) {
					Spring *s = springAdd();
					s->verts[0] = &cells[cellCount].verts[j];
					s->verts[1] = &cells[cellCount].verts[k];
					FVec3 p = s->verts[0]->pos;
					p.sub( s->verts[1]->pos );
					s->currNatLen = p.mag();
					s->origNatLen = s->currNatLen;
					s->targNatLen = s->currNatLen;
					s->breakable = 0;
					s->cell = &cells[cellCount];
					s->type = INTERIOR;
					s->color[0] = 0.65f;
					s->color[1] = 0.65f;
					s->color[2] = 0.65f;
					if( k==j+1 || (j==0 && k==sides-1) ) {
						s->color[0] = 1.f;
						s->color[1] = 0.f;
						s->color[2] = 0.f;
					}
				}
			}

			cellCount++;
			assert( cellCount < MAX_CELLS );
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
	
	if( zmsgIs(type,Key) && zmsgIs(which,w) ) {
		cellReproduce( &cells[0] );
	}
	
	if( zmsgIs(type,Key) && zmsgIs(which,e) ) {
		for( int i=0; i<MAX_CELLS; i++ ) {
			if( cells[i].active && cells[i].selected ) {
				if( cells[i].squished ) {
					cellNormalize( &cells[i] );
				}
				else {
					cellSquish( &cells[i] );
				}
			}
		}
	}
	
	if( zmsgIs(type,ZUIMouseClickOn) && zmsgIs(dir,D) && zmsgIs(which,L) && zmsgIs(shift,1) ) {
		selectedSpring = springFindClosest( mouse );
		adjusting = mouse.y;
		adjustingStartLen = selectedSpring->currNatLen;
	}
	else {
		if( zmsgIs(type,ZUIMouseClickOn) && zmsgIs(dir,D) && zmsgIs(which,L) ) {
			for( int i=0; i<MAX_CELLS; i++ ) {
				if( cells[i].active && cells[i].inside(mouse) ) {
					cells[i].selected ^= 1;
					dragging = &cells[i];
					break;
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


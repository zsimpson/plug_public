#include "_gcode/tool.h"
#include "_gcode/geom.h"
#include "_gcode/gcode.h"

extern void trace(char* fmt, ...);


void ToolPath::add( DVec3 pt )
{	
	instructions.add( new ToolInstruction( pt, 10 ) ); 


}

ToolInstruction::ToolInstruction( DVec3 pos, double speed )
{
	this->pos = pos;
	this->speed = speed;

}

ToolPath::ToolPath()
{
	
}


ToolPath* toolPathGen_GeomShape( GeomShape *shape ) {
	
	// @TODO Add Speed control. 
	 
	// Evetually ToolPath will not only be a container to hold instruction
	// but will also know things about the material so that this generator
	// can be smart about speeds and bits, etc.
	ToolPath* tempPath = new ToolPath(); 
	
	//start computing tool path
	double toolRadius = 1.0 / 16;
	double moveHeight = .1;
	double passes = 1; 
	double totalDepth = -(1.0 / 16.0);
	double plunge = (totalDepth / passes);
	for( int nPoly = 0; nPoly < shape->polyCount(); nPoly = nPoly + 1 )
	{
		trace("nPoly %d \n", nPoly);

		for(double depth = plunge; depth >= totalDepth; depth += plunge )
		{
			trace("depth %d \n", depth);
			GeomPoly* outline = shape->polys[nPoly];
			int d;
			double sum = 0.0;
			for( d=0; d<outline->vertCount(); d++ ) {
				//SOLVE for d0
				DVec3 d0 = outline->verts[(d + 1) % outline->vertCount()];
				d0.sub(outline->verts[d]);

				//SOLVE for d1
				DVec3 d1 = outline->verts[(d + 2) % outline->vertCount()];
				d1.sub(outline->verts[(d + 1) % outline->vertCount()]);

				DVec3 _d0( d0 );
				DVec3 _d1( d1 );

				_d0.cross( _d1 );
				sum+=_d0.z;
			}

			double sign = sum > 0.0 ? -1.0 : 1.0;
			
			/* //Use this if winding breaks.
			if(manwind)
			{
				if(wind)
					sign = 1.0;
				else
					sign = -1.0;
			}
			*/

			for( d=0; d<outline->vertCount(); d++ ) 
			{
				//SOLVE for d0
				DVec3 d0 = outline->verts[(d + 1) % outline->vertCount()];
				d0.sub(outline->verts[d]);

				//SOLVE for d1
				DVec3 d1 = outline->verts[(d + 2) % outline->vertCount()];
				d1.sub(outline->verts[(d + 1) % outline->vertCount()]);

				//SOLVE for v0
				DVec2 v0 = d0;
				v0.perp();
				v0.normalize();
				v0.mul( sign * toolRadius );

				//SOLVE for v1
				DVec2 v1 = d1;
				v1.perp();
				v1.normalize();
				v1.mul( sign * toolRadius );

				//SOLVE for p0
				DVec2 p0 = outline->verts[d];
				p0.add( v0 );														 

				//SOLVE for p1
				DVec2 p1 = outline->verts[(d+1)%outline->vertCount()];
				p1.add( v1 );
				//COMPUTE where the two lines meet to find the next vert for the toolPath
				DMat2 m;
				m.m[0][0] = d0.x;
				m.m[0][1] = d0.y;
				m.m[1][0] = -d1.x;
				m.m[1][1] = -d1.y;

				DVec2 b = DVec2( -p0.x + p1.x, -p0.y + p1.y );
						  
				if( m.inverse() ) {
					DVec2 x = m.mul( b );
					DVec2 point( p0.x + (x.x * d0.x), p0.y + (x.x * d0.y) );
					bool notToClose = true;
					for( int nPoly2 = 0; nPoly2 < shape->polyCount(); nPoly2++ )
					{
						trace("nPoly2 %d \n", nPoly2);

							for( int d2=0; d2 < shape->polys[nPoly2]->vertCount(); d2++ ) 
							{
								DVec2 dist = shape->polys[nPoly2]->verts[d2];
								dist.sub(point);
								double dista = dist.mag();
								if(dista < toolRadius - .001)
								{
									notToClose = false;
								}
							}
						
					}

					if(d == 0)
					{
						tempPath->add( DVec3( point.x, point.y, moveHeight ) );
					}
					if( notToClose )
					{
						tempPath->add( DVec3( point.x, point.y, depth ) );
					}
				}

			}
		}
		tempPath->add( DVec3( tempPath->instructions[tempPath->instructions.count - 1]->pos.x, tempPath->instructions[tempPath->instructions.count - 1]->pos.y, moveHeight) );
	}
	
	return tempPath;

}

bool inline LineIntersectLine( DVec2 v1, DVec2 v2, DVec2 v3, DVec2 v4 )
{
    double denom = ((v4.y - v3.y) * (v2.x - v1.x)) - ((v4.x - v3.x) * (v2.y - v1.y));
    double numerator = ((v4.x - v3.x) * (v1.y - v3.y)) - ((v4.y - v3.y) * (v1.x - v3.x));

    double numerator2 = ((v2.x - v1.x) * (v1.y - v3.y)) - ((v2.y - v1.y) * (v1.x - v3.x));

    if ( denom == 0.0f )
    {
        if ( numerator == 0.0f && numerator2 == 0.0f )
        {
            return false;//COINCIDENT;
        }
        return false;// PARALLEL;
    }
    double ua = numerator / denom;
    double ub = numerator2/ denom;

    return (ua >= 0.0f && ua <= 1.0f && ub >= 0.0f && ub <= 1.0f);
}

bool LineIntersectsRect( const DVec2 &v1, const DVec2 &v2, float x, float y, float width, float height )
{
        FVec2 lowerLeft( x, y+height );
        FVec2 upperRight( x+width, y );
        FVec2 upperLeft( x, y );
        FVec2 lowerRight( x+width, y+height);
        // check if it is inside
        if (v1.x > lowerLeft.x && v1.x < upperRight.x && v1.y < lowerLeft.y && v1.y > upperRight.y &&
            v2.x > lowerLeft.x && v2.x < upperRight.x && v2.y < lowerLeft.y && v2.y > upperRight.y )
        {   
            return true;
        }
        // check each line for intersection
        if (LineIntersectLine(v1,v2, upperLeft, lowerLeft ) ) return true;
        if (LineIntersectLine(v1,v2, lowerLeft, lowerRight) ) return true;
        if (LineIntersectLine(v1,v2, upperLeft, upperRight) ) return true;
        if (LineIntersectLine(v1,v2, upperRight, lowerRight) ) return true;
        return false;
}



void lineBresenham(int x0, int y0, int x1, int y1, char* grid)
{
    const int size = 64;
	int dy = y1 - y0;
    int dx = x1 - x0;
    int stepx, stepy;

    if (dy < 0) { dy = -dy;  stepy = -1; } else { stepy = 1; }
    if (dx < 0) { dx = -dx;  stepx = -1; } else { stepx = 1; }
    dy <<= 1;                                                  // dy is now 2*dy
    dx <<= 1;                                                  // dx is now 2*dx

    grid[x0+y0*size] = 1;
    if (dx > dy) {
        int fraction = dy - (dx >> 1);                         // same as 2*dy - dx
        while (x0 != x1) {
            if (fraction >= 0) {
                y0 += stepy;
                fraction -= dx;                                // same as fraction -= 2*dx
            }
            x0 += stepx;
            fraction += dy;                                    // same as fraction -= 2*dy
            grid[x0+y0*size] = 1;
        }
    } else {
        int fraction = dx - (dy >> 1);
        while (y0 != y1) {
            if (fraction >= 0) {
                x0 += stepx;
                fraction -= dy;
            }
            y0 += stepy;
            fraction += dx;
            grid[x0+y0*size] = 1;
        }
    }
}


int lToG( double n, double scale )
{						
	return (int)(n / scale);
}
int depth = 0;
void floodFill( char* grid, int x, int y )
{
	depth++;
	int size = 64;
	if( x >= 0 && x < size && y >= 0 && y < size && depth < 1000 )
	{
		if( grid[ (y*size) + x ] == 0 )
		{
			grid[ (y*size) + x ] = 1;
			floodFill( grid, x + 1, y );
			floodFill( grid, x, y + 1 );
			floodFill( grid, x - 1, y );
			floodFill( grid, x, y - 1 );
		}
	}
	depth--;
}

ToolPath* toolPathGen_Excavate( GeomShape *shape )
{
	ToolPath* newPath = new ToolPath();
	int a = 0;
	int const size = 64;
	float depth = (1.0 / 16.0);
	float scale = 2.5f / (float)size;
	double bitSize = 1.f / 16.f;
	char* grid = (char*)malloc(size*size); // 0 - clear, 1 - not clear, 2 - clear and checked, 3 - not clear and checked 
	char newGrid[size][size] = {0,}; //1 = good, 0 = bad
	
	for(int n = 0; n < size*size; n++)
	{
		grid[n] = 0;
	}

	//SETUP grid
	
	for(int y = 0; y < size; y++)
	{
		trace ("setupy %d \n", y );

		for(int x = 0; x < size; x++)
		{
			for(int i = 0; i < shape->polys.count; i++)
			{
				for(int j = 0; j < shape->polys[i]->verts.count - 1; j++)
				{
					DVec2 b1 = DVec2( x * scale - bitSize, y * scale - bitSize );
					DVec2 b2 = DVec2( x * scale + bitSize, y * scale - bitSize );
					DVec2 b3 = DVec2( x * scale - bitSize, y * scale + bitSize );
					DVec2 b4 = DVec2( x * scale + bitSize, y * scale + bitSize );
					if(	shape->lineIntersect( b1, b2 ) ||
						shape->lineIntersect( b2, b3 ) ||
						shape->lineIntersect( b3, b4 ) ||
						shape->lineIntersect( b4, b1 ) ||
						shape->isInside( DVec2(x * scale, y * scale) ))
					{
						grid[x+y*size] = 1;
					}
					else
					{
						grid[x+y*size] = 0;
					}
				}
			}
		}
	}
	
	//@TODO: Bresenham algorithm is quick, but not accurate. This is because it iterates along an axis and approximates. 
	//		 Should be replaced.
	
//	for(int i = 0; i < shape->polys.count; i++)
//	{
//		for(int j = 0; j < shape->polys[i]->verts.count; j++)
//		{
//			lineBresenham(	lToG(shape->polys[i]->verts[j].x, scale), 
//						lToG(shape->polys[i]->verts[j].y, scale), 
//						lToG(shape->polys[i]->verts[(j + 1) % shape->polys[i]->verts.count].x, scale), 
//						lToG(shape->polys[i]->verts[(j + 1) % shape->polys[i]->verts.count].y, scale), 
//						grid );
//		}
//	}
//
//	DVec2 pt;
//	IVec2 pti;
//	bool found = false;
//	for(int y = 0; y < size; y++)
//	{
//		for( int x = 0; x < size; x++ )
//		{
//			pt = DVec2( x * scale, y * scale );
//			if(	shape->isInside( pt ) && grid[x+y*size] == 0 )
//			{
//				grid[x+y*size] = shape->isInside( DVec2(x * scale, y * scale) );	
//			}
//		}
//	}
//	
//
	for(y = 0; y < size; y++ )
	{
		trace ("y %d", y );
		for( int x = 0; x < size; x++ )
		{
			double z;
			if( grid[x+y*size] == 1 )
			{
				z = depth;
			}
			else
			{
				z = -depth;
			}
			newPath->add( DVec3( x * scale, y * scale, z ) );
			if( grid[x+1+y*size] != grid[x+y*size] )
			{
			   newPath->add( DVec3( x * scale, y * scale, -z ) );
			} 	
		}
		y++;
		for( x = size - 1; x >= 0; x-- )
		{
			double z;
			if( grid[x+y*size] == 1 )
			{
				z = depth;
			}
			else
			{
				z = -depth;
			}
			newPath->add( DVec3( x * scale, y * scale, z ) );
			if( grid[(x-1)+y*size] != grid[x+y*size] )
			{
			   newPath->add( DVec3( x * scale, y * scale, -z ) );
			}
		}
	}
	
	
	newPath->add( DVec3( 1.0, 1.0, 1.0 ) );

	ToolPath* second = toolPathGen_GeomShape( shape );
	newPath->instructions.addStealOwnershipFrom( second->instructions );
	
	return newPath;	
}






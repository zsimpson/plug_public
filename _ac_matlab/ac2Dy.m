function dy = ac2DY( t, y, acNet )
    nodeCount = acNet.nodeCount;
    dim = acNet.dim;

    % COMPUTE the force on each node by calling the function pointer that
    % defines the network and then apply diffusion
    dy = zeros( nodeCount * dim * dim, 1 );
    for( yc = 0:dim-1 )
        for( xc = 0:dim-1 )

            i = yc*nodeCount*dim + xc*nodeCount;
            j = i + nodeCount;
            
            acNet.currentX = xc / dim;
            acNet.currentY = yc / dim;

            % CALL the DY function that describes the specific network
            dy(i+1:j) = acNet.dyFunc( acNet, y(i+1:j) );

            % DIFFUSE circular and apply capacitance correction
            if(acNet.bounded==1)
                for( k = 1:nodeCount )
                    if yc == 0
                        top = y((yc  )*nodeCount*dim + nodeCount*xc +k );
                    else
                        top = y((yc-1)*nodeCount*dim + nodeCount*xc +k );
                    end

                    if yc == dim-1
                        bot = y((yc  )*nodeCount*dim + nodeCount*xc +k );
                    else
                        bot = y((yc+1)*nodeCount*dim + nodeCount*xc +k );
                    end

                    if xc == 0
                        lft = y(yc*nodeCount*dim + nodeCount*(xc  ) +k );
                    else
                        lft = y(yc*nodeCount*dim + nodeCount*(xc-1) +k );
                    end

                    if xc == dim-1
                        rgt = y(yc*nodeCount*dim + nodeCount*(xc  ) +k );
                    else
                        rgt = y(yc*nodeCount*dim + nodeCount*(xc+1) +k );
                    end

                    dy(i+k) = dy(i+k) + acNet.diff(k) * ( lft - y(i+k) );
                    dy(i+k) = dy(i+k) + acNet.diff(k) * ( rgt - y(i+k) );
                    dy(i+k) = dy(i+k) + acNet.diff(k) * ( top - y(i+k) );
                    dy(i+k) = dy(i+k) + acNet.diff(k) * ( bot - y(i+k) );
                    dy(i+k) = dy(i+k) / acNet.capacitance;
                end
            else
                xm = mod( xc-1+dim, dim );
                xp = mod( xc+1+dim, dim );
                ym = mod( yc-1+dim, dim );
                yp = mod( yc+1+dim, dim );

                for( k = 1:nodeCount )
                    dy(i+k) = dy(i+k) + acNet.diff(k) * ( y(yc*nodeCount*dim + nodeCount*xm +k ) - y(i+k) );
                    dy(i+k) = dy(i+k) + acNet.diff(k) * ( y(yc*nodeCount*dim + nodeCount*xp +k ) - y(i+k) );
                    dy(i+k) = dy(i+k) + acNet.diff(k) * ( y(ym*nodeCount*dim + nodeCount*xc +k ) - y(i+k) );
                    dy(i+k) = dy(i+k) + acNet.diff(k) * ( y(yp*nodeCount*dim + nodeCount*xc +k ) - y(i+k) );
                    dy(i+k) = dy(i+k) / acNet.capacitance;
                end
            end
        end
    end
    
end

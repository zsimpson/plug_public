function dy = ac1DY( t, y, acNet )
    nodeCount = acNet.nodeCount;
    xDim = acNet.xDim;

    % COMPUTE the force on each node by calling the function pointer that
    % defines the network and then apply diffusion and pull down
    dy = zeros( nodeCount * xDim, 1 );
    for( x = 0:xDim-1 )

        i = x*nodeCount+1;
        j = i+nodeCount-1;
        
        % CALL the DY function that describes the specific network
        dy(i:j) = acNet.dyFunc( acNet, y(i:j) );

        % DIFFUSE circular and apply capacitance correction
        xm = mod( x-1+xDim, xDim );
        xp = mod( x+1+xDim, xDim );
        idx = x*nodeCount;
        for( k = 1:nodeCount )
            dy(idx+k) = dy(idx+k) + acNet.diff(k) * ( y(xm*nodeCount+k) - y(idx+k) );
            dy(idx+k) = dy(idx+k) + acNet.diff(k) * ( y(xp*nodeCount+k) - y(idx+k) );
            dy(idx+k) = dy(idx+k) / acNet.capacitance;
        end
    end

    % PULL down each node
%    dy = dy - y * acNet.pullDown;
end

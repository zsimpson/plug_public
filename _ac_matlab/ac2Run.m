function acNet = ac2Run( acNet, duration )
    % The initial conditions are stored in a list of spatial matricies
    dim = acNet.dim;
    nc = acNet.nodeCount;
    icRowVec = zeros( 1, dim * dim * nc );
    for y=0:dim-1
        for x=0:dim-1
            for i=0:nc-1
                icRowVec(y*dim*nc+x*nc+i+1) = acNet.ICs{i+1}(y+1,x+1);
            end
        end
    end
    
    [acNet.T,acNet.Y] = ode45( @ac2Dy, [0 duration], icRowVec, [], acNet );
end


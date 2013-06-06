function acNet = ac2Create( dim, dyFunc )
    nodeCount_gateCount = dyFunc();
    acNet.nodeCount = nodeCount_gateCount(1);
    acNet.gateCount = nodeCount_gateCount(2);
    acNet.dim = dim;
        % This is the dimension of one side of the square
    acNet.diff = ones( acNet.nodeCount, 1 );
    for i=1:acNet.nodeCount
        acNet.ICs{i} = zeros( dim, dim );
    end
    acNet.capacitance = 1;
    acNet.dyFunc = dyFunc;
    acNet.gateConc = ones( 1, acNet.gateCount );
    acNet.pullDown = 1;
end


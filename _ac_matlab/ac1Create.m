function acNet = ac1Create( xDim, dyFunc )
    nc_gc = dyFunc();
    acNet.nodeCount = nc_gc(1);
    acNet.gateCount = nc_gc(2);
    acNet.xDim = xDim;
    acNet.diff = ones( acNet.nodeCount, 1 );
    acNet.ICs = zeros( acNet.nodeCount, xDim );
    acNet.capacitance = 1;
    acNet.dyFunc = dyFunc;
    acNet.gateConc = ones( 1, acNet.gateCount );
    acNet.pullDown = 1;
end


function acNet = ac1CreateDiff( acNet, method, parameter )
    nc = acNet.nodeCount;
    diff = zeros( nc, 1 );
    switch method
        case 'rand-strong',
    		diff = rand( nc, 1 );
        case 'rand-similar',
    		diff = 0.5 + rand( nc, 1 ) / 5;
        case 'one-full',
    		diff = zeros( nc, 1 );
            diff(1) = 1;
        case 'half-full',
    		diff = zeros( nc,1 );
            diff( 1:nc, 1 ) = ones( nc, 1 );
        case 'all-full',
    		diff = ones( nc, 1 );
        case 'all-same',
    		diff = ones( nc, 1 ) * parameter;
	end
	acNet.diff = diff;
end


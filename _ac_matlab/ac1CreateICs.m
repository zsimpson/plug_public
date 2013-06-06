function acNet = ac1CreateICs( acNet, method, parameter1, parameter2, parameter3, parameter4 )
    nc = acNet.nodeCount;
    xd = acNet.xDim;
	ICs = zeros( nc, xd );
    switch method
        case 'zeros',
            ICs = zeros( nc, xd );
        case 'ones',
            ICs = ones( nc, xd );
        case 'minus-ones',
            ICs = -ones( nc, xd );
        case 'test',
            ICs = [ ones( 1, xd ); -ones( 1, xd ) ];
        case 'noise-strong',
            ICs = rand( nc, xd ) - 0.5;
        case 'noise-weak',
            ICs = (rand( nc, xd )-0.5) / 30;
        case 'gaussian-one',
            ICs = zeros( nc, xd );
            for( x=0:xd-1 )
                xf = x / xd;
                ICs(1,x+1) = ICs(1,x+1) + parameter2 * exp( -(xf-0.5)*parameter1 .* (xf-0.5)*parameter1 );
            end
        case 'gaussian-one-plus-noise',
            ICs = (rand( nc, xd )-0.5) / 50;
            for( x=0:xd-1 )
                xf = x / xd;
                ICs(1,x+1) = ICs(1,x+1) + parameter2 * exp( -(xf-0.5)*parameter1 .* (xf-0.5)*parameter1 );
            end
        case 'uniform-specified',
            ICs = repmat( parameter1', 1, xd );
        case 'gaussian-add',
            % parameter1 is the width of the Gaussian
            % parameter2 is the size of the Gaussian
            % parameter3 is the normalized x position
            % parameter4 is which reagent to add to
            
            ICs = acNet.ICs;
            for( x=0:xd-1 )
                xf = x / xd;
                ICs(parameter4,x+1) = acNet.ICs(parameter4,x+1) + parameter2 * exp( -(xf-parameter3)*parameter1 .* (xf-parameter3)*parameter1 );
            end
        case 'add-spot',
            % parameter1 is which node to add to
            % parameter2 is how much
            % parameter3 is where in space
            ICs = acNet.ICs;
            ICs(parameter1,parameter3) = acNet.ICs(parameter1,parameter3) + parameter2;
    end
	acNet.ICs = ICs;
end


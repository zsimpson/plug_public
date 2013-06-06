%global acNet;

function acNet = ac2Test1()
    %this is a test comment to see if subversion is working
    acNet = ac2Test_EdgeBoundTest();
end

function acNet = ac2Test_EdgeBoundTest()
    dim = 30;
    acNet = ac2Create( dim, @ac2_NoOp_1_DY );
    acNet.diff = [ 1 ];
    
    image=(importdata('ieast30.bmp'));
    image=image.cdata;
    acNet = ac2CreateICs( acNet, 'add-mat', 1, image );
                   
    duration = 10;
    acNet.bounded = 1;
    acNet = ac2Run( acNet, duration );

    mov = ac2View( acNet, 1 );
    movie2avi( mov, 'c:/transfer/test.avi' );
end

function acNet = ac2Test_EdgeTest()
    dim = 100;
    acNet = ac2Create( dim, @ac2_Edge_DY );
    acNet.diff = [ 0.1; 0.0 ];
    acNet.parameters = [ 0.1 0.05 0.1 0.05 ];

    duration = 200;
    acNet = ac2Run( acNet, duration );

    ac2View( acNet, 1 );
end

function acNet = ac2Test_DiffTest()
    dim = 100;
    acNet = ac2Create( dim, @ac2_NoOperation_DY );
    acNet = ac2CreateICs( acNet, 'add-spot', 1, 1.0, dim/2, dim/2 );
    acNet.diff = [ 0.1; 0.1 ];

    duration = 10;
    acNet = ac2Run( acNet, duration );

    ac2View( acNet, 1 );
end

function dy = ac2_Edge_DY( acNet, y )
	% Test pulse
    if nargin == 0
        dy = [2 0];
        return;
    end
    deltaX = acNet.currentX - 0.5;
    deltaY = acNet.currentY - 0.5;
    currentRadius2 = deltaX^2 + deltaY^2;
    signal = 1;
    notSignal = 0;
    if currentRadius2 > 0.3^2 
        signal = 0;
        notSignal = 1;
    end
    dy(1) = acNet.parameters(1) * signal - acNet.parameters(2) * y(1);
    dy(2) = notSignal * acNet.parameters(3) * y(1) - acNet.parameters(4) * y(2);
end

function dy = ac2_StadiumWave_DY( acNet, y )
	% Test pulse
    if nargin == 0
        dy = [2 5];
        return;
    end

    gateOutput(1) = -1;
    gateOutput(2) = -1;
    gateOutput(3) = gateModelIpOp( y(1) );
    gateOutput(4) = gateModelIpOp( y(1) );
    gateOutput(5) = gateModelIpOm( y(2) );
    
    gateOutput = gateOutput .* acNet.gateConc;

    dy(1) = -acNet.pullDown*y(1) + gateOutput(1) + gateOutput(3) + gateOutput(5);
    dy(2) = -acNet.pullDown*y(2) + gateOutput(2) + gateOutput(4);
end

function dy = ac2_NoOperation_DY( acNet, y )
	% Test pulse
    if nargin == 0
        dy = [2 5];
        return;
    end
    dy = [ 0 0 ];
end

function dy = ac2_NoOp_1_DY( acNet, y )
	% Test pulse
    if nargin == 0
        dy = [1 0];
        return;
    end
    dy = [ 0 ];
end

function production = gateModelImOp( inputConc )
    production = +( tanh(-10*inputConc+2.5)/2+0.5 );
end

function production = gateModelIpOp( inputConc )
    production = +( tanh(+10*inputConc+2.5)/2+0.5 );
end

function production = gateModelImOm( inputConc )
    production = -( tanh(-10*inputConc+2.5)/2+0.5 );
end

function production = gateModelIpOm( inputConc )
    production = -( tanh(+10*inputConc+2.5)/2+0.5 );
end

function colorMapMinusToPlus()
    rRamp = 0.2:0.05:1;
    bRamp = 1:-0.05:0.2;
    colors = [ 0*bRamp' 0*bRamp' bRamp'; rRamp' 0*rRamp' 0*rRamp' ];
    colormap( colors );
end



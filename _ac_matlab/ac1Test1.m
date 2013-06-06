% How to use
% This simulates a 1D lattice of reactions
%
% net = ac1Create( width, dyFunc )
%   DESCRIPTION: used to construct the system
%     width = number of bins (example, 128)
%     DY function = defines the callback DY function, see as an example: ac1_StadiumWave_DY
%
% ac1CreateICs( net, mode_string )
%   DESCRIPTION: used to set initial conditions on the system
%     Note that some of the conditions are called "add-" so you
%     you can have more than one call to ac1CreateICs
%       net = the network
%       method = one of several patterns, see ac1CreateICs.m
%
% function net = ac1CreateDiff( net, method, parameter )
%   DESCRIPTION: used to set diffusion terms
%     net = the network
%     method = one of several patterns, see ac1CreateDiff.m
%
% function net = ac1DoubleSpatialRes( net )
%   DESCRIPTION: doubles the sapatial resolution. You should get the same
%     answer but at higher res
%       net = the network
%
% function dy = ac1DY( t, y, net )
%   DESCRIPTION: The heart of the integrator, calls the callback function
%     and implementes diffusion.  This function is called by the ac1Run
%     function so you don't need to use it directly.
%       t = time
%       y = current values
%       net = the network system
%     
% function Y = ac1ExtractReagent( net, which, withExtraCol )
%   DESCRIPTION: Extracts a single reagent out of the system for viewing
%     purposes.
%       net = the network
%       which = which reagent to extract
%       withExtraCol = boolean that if true adds a column so that the pcolor
%       function will behave nicely
%     
% function ac1QuiverView( net, arrowCount, arrowScale, view )
%   DESCRIPTION: Creates a quiver plot of the first two variables in the 
%     system.
%       net = the network 
%       arrowCount = how many arrows on a side
%       arrowScale = they're size
%       view = an array [left right bottom top]
%
% function net = ac1Run( net, duration )
%   DESCRIPTION: Runs the simulation for duration
%       net = the network 
%       duration = the duration
%
% function ac1View( net, method, which )
%   DESCRIPTION: Produces a space time plot.
%     net = the network
%     method = one of seveal methods, see the function
%     which = a vector of which reagent you want to see
%
% function ac1Test1()
%   DESCRIPTION: This file. Is where you setup the system in various ways
%     and see what happens!  At the bottom of the file are a lot of little
%     utility functions I used for examining the system


function acNet = ac1Test1()
    %acNet = ac1Test_StableWave();
    acNet = ac1Test_DoubleDiffTest();
end

function acNet = ac1Test_DoubleDiffTest()
    width = 70;
    acNet = ac1Create( width, @ac1_SpecificAssociation_DY );
    acNet = ac1CreateICs( acNet, 'zeros' );
    acNet = ac1CreateICs( acNet, 'add-spot', 1, 2.0, round(width/4) );
    acNet = ac1CreateICs( acNet, 'add-spot', 2, 2.0, round((3*width)/4));
    acNet = ac1CreateICs( acNet, 'add-spot', 3, 2.0, [1:width] );
    %acNet = ac1CreateICs( acNet, 'add-spot', 4, 0.05, [1:50] );

    acNet.diff = [ 0.1; 0.1; 0 ; 0];

    duration = 1000;
    acNet = ac1Run( acNet, duration );

    ac1View( acNet, 'lines-SpecificAssociation', 900 );
    title( 'Diffusion' );
end


function dy = ac1_SpecificAssociation_DY( acNet, y )
    if nargin == 0
        dy = [4 0];
        return;
    end
    dy = [ 0 0 0 0 ];
    dy(2)=-(y(2)*y(3)*1)+(y(4)*1);
    dy(3)=-(y(2)*y(3)*1)+(y(4)*1);  
    dy(4)=(y(2)*y(3)*1)-(y(4)*1);  
    %dy = [ 0 0 0 0 ];
end


function dy = ac1_NoOperation_DY( acNet, y )
    if nargin == 0
        dy = [4 0];
        return;
    end
    dy = [ 0 0 0 0 ];
end


function acNet = ac1Test_StableWave()
    width = 50;
    acNet = ac1Create( width, @ac1_StadiumWave_DY );
    acNet = ac1CreateICs( acNet, 'minus-ones' );
    acNet = ac1CreateICs( acNet, 'add-spot', 1, 1.0, 32 );
        % Add the standing spot
    %acNet = ac1CreateICs( acNet, 'add-spot', 2, 25.4, 48 );
        % Add the resting spot

	% Stable wave parameters
    acNet.diff = [ 0.1; 0.1 ];
    acNet.pullDown = 10*9.05e-3;
    acNet.gateConc = [ 10*6.19e-3 10*1.42e-2 10*3.39e-2 10*2.24e-2 10*4.14e-2 ];
    view = [ -2 +2 -2 0.5 ];

    % Pattern forming parameters
    %acNet.diff = [ 0.1; 0.1 ];
    %acNet.pullDown = 0.5;
    %acNet.gateConc = [ 0.3 0.5 1 0.28 80 ]; % Pattern forming! (when diff = 0.1, pulldown=0.5)
    %view = [ -2 +2 -2 0.5 ];

    % Multi-stable transition 1
    %acNet.diff = [ 0.1; 0.1 ];
    %acNet.pullDown = 10*9.05e-3;
    %acNet.gateConc = [ 10*6.44e-3 10*2.69e-2 10*3.39e-2 10*2.29e-2 10*3.60e-2 ];
    %view = [ -2 +5 -5 0.5 ];

    % Multi-stable transition 2
    %acNet.diff = [ 0.1; 0.1 ];
    %acNet.pullDown = 10*9.05e-3;
    %acNet.gateConc = [ 10*6.44e-3 10*2.39e-2 10*3.39e-2 10*2.29e-2 10*3.60e-2 ];
    %view = [ -2 +5 -5 0.5 ];

    % Stable oscillator
    %acNet.diff = [ 0.1; 0.1 ];
    %acNet.pullDown = 10*9.05e-3;
    %acNet.gateConc = [ 0 10*1.42e-2 10*3.39e-2 10*2.24e-2 10*4.14e-2 ];
    %view = [ -2 +2 -2 0.5 ];

    % Pattern forming parameters
    % There's two equilibrium in Q3 and Q4. There's a huge shear in Q1 & Q2
    %acNet.diff = [ 0.1; 0.1 ];
    %acNet.pullDown = 0.5;
    %acNet.gateConc = [ 0.3 0.5 1 0.28 50 ]; % Pattern forming! (when diff = 0.1, pulldown=0.5)
    %view = [ -2 +2 -2 0.5 ];

    % Flip flop (Q1 & Q3 equilibirums)
    %acNet.diff = [ 0.1; 0.1 ];
    %acNet.pullDown = 10*8.1e-3;
    %acNet.gateConc = [ 10*4.97e-3 10*2.8e-2 10*3.75e-2 10*3.34e-2 10*3.01e-2 ];
    %view = [ -2 +3 -4 3 ];

    tracePoints = [25 32];
    duration = 100;
    acNet = ac1Run( acNet, duration );

    % PLOT stand
    subplot( 3, 2, 1 );
    ac1View( acNet, 'one-nosubplot', 1 );
    colorMapMinusToPlus();
    caxis( [ -1 1 ] );
    title( 'stand' );

    % DRAW lines on the space-time plot to show where the traces are
    colors = get(gca,'ColorOrder');
    for i=1:size(tracePoints,2)
        line( [ 0.5+tracePoints(i) 0.5+tracePoints(i) ], [ 0 1000 ], 'LineWidth', 2, 'Color', colors(i,:) );
        hold on;
        plot( 0.5+tracePoints(i), 0, 'k*', 'MarkerSize', 10 );
        hold off;
    end

    % PLOT tired
    subplot( 3, 2, 2 );
    ac1View( acNet, 'one-nosubplot', 2 );
    colorMapMinusToPlus();
    caxis( [ -1 1 ] );
    title( 'tired' );

    % PLOT phase
    subplot( 3, 2, [3 4 5 6] );
    ac1PhaseView( acNet, tracePoints, 1, 2, view );
    hold on;
    ac1QuiverView( acNet, 50, 1, view );
	hold off;
end


function dy = ac1_StadiumWave_DY( acNet, y )
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


%{
function ac1Test_TiredEvolution( p1, p2 )
    % NOTE: This graph requires a hack in ac1DY to eliminate pull down on node 1
    
    % I'm really only interested in the question: when standing is 5, what
    % does the charging of tired look like?
    dur = 200;
    acNet = ac1Create( 1, @ac1_TiredOnly_DY );
    acNet.pullDown = 0.05;
    acNet.gateConc = [ p1 p2 ];
    
    % The steady state without the charing circuit is the ratio of the pull
    % up gate to the resistor.  Start it at that value. - p1 / acNet.pullDown
    acNet.ICs = [ -p1/acNet.pullDown ];
    acNet.standing = 0;
    acNet = ac1Run( acNet, dur );
    trace = interp1( acNet.T, acNet.Y(:,1), linspace(0,acNet.T(end),4096) );

    plot( linspace(0,dur,4096), trace );
    %axis( [ 0 dur 0 6 ] );
    axis( [ 0 100 -20 20 ] );
    set( gca(), 'YGrid', 'on' );
    set( gca(), 'YTick', [0] );
    %xlabel( 'time' );
    %ylabel( '[tired]' );
    %title( sprintf('Steady-state response of "tired" given "standing" input\np1=%4.2f p2=%4.2f',p1,p2) );
    title( sprintf('p1=%4.2f p2=%4.2f',p1,p2) );
end

function acNet = ac1Test_TiredEvolutionParameterGrid()
    for i=1:8
        p1 = linspace( 0.1, 1.0, 8 ); p1 = p1(i);
        for j=1:8
            p2 = linspace( 0.1, 1.0, 8 ); p2 = p2(j);

            subplot( 8, 8, (i-1)*8+j );
            ac1Test_TiredEvolution( p1, p2 )
            
            drawnow();
        end
    end
end

function ac1Test_TiredSteadyState( p1, p2 )
    for i=1:100
        ic = linspace( -5, 5, 100 ); ic = ic(i);
        acNet = ac1Create( 1, @ac1_TiredOnly_DY );
        acNet.pullDown = 0.05;
        acNet.gateConc = [ p1 p2 ];
        acNet.ICs = [ ic 0 ];
        acNet = ac1Run( acNet, 1000.0 );
        %trace(:,i) = interp1( acNet.T, acNet.Y(:,2), linspace(0,acNet.T(end),4096) );
        X(i) = ic;
        Y(i) = acNet.Y(end,2);
    end
    plot( X, Y );
    xlabel( '[standing]' );
    ylabel( '[tired]' );
    title( sprintf('Steady-state response of "tired" given "standing" input\np1=%4.2f p2=%4.2f',p1,p2) );
end

function ac1Test_PlotGateModel()
    X = -2:0.01:2;
    Y = gateModelIpOp( X );
    plot( X, Y );
    axis( [ -2 2 -0.1 1.1 ] );
    set( gca(), 'XGrid', 'on' );
    set( gca(), 'XTick', [-1 0 1] );
    xlabel( '[input]' );
    ylabel( 'd [input] / dt' );
    title( 'Unitless gate model for gate I+O+' );
end

function ac1Test_StandingFeedbackDerivPlot( p1, p2 )
    ics = linspace( -15, 15, 500 ); 
    dy = [];
    for i=1:500
        ic = ics(i);
        acNet = ac1Create( 1, @ac1_FeedbackOnly_DY );
        acNet.pullDown = 0.05;
        acNet.gateConc = [ p1 p2 ];
        acNet.ICs = [ ic ];
        acNet = ac1Run( acNet, 2.0 );
        dy(i) = ac1DY( 0, acNet.ICs, acNet );
    end
    plot( ics, dy );
    set( gca(), 'XGrid', 'on' );
    set( gca(), 'XTick', [-10 0 10] );
    set( gca(), 'YGrid', 'on' );
    set( gca(), 'YTick', [0] );
    xlabel( '[standing]' );
    ylabel( 'd [standing] / dt' );
    title( sprintf('Derivative of standing feedback circuit\np1=%4.2f p2=%4.2f',p1,p2) );
end

function ac1Test_StandingFeedbackEvolution( p1, p2 )
    dur = 110.0;
    for i=1:100
        ic = linspace( -30, 30, 100 ); ic = ic(i);
        acNet = ac1Create( 1, @ac1_FeedbackOnly_DY );
        acNet.pullDown = 0.05;
        acNet.gateConc = [ p1 p2 ];
        acNet.ICs = [ ic ];
        acNet = ac1Run( acNet, dur );
        trace(:,i) = interp1( acNet.T, acNet.Y(:,1), linspace(0,acNet.T(end),4096) );
    end
    plot( linspace(0,dur,4096), trace );
    axis( [ 0 dur -30 30 ] );
    xlabel( 'time' );
    ylabel( '[standing]' );
    title( sprintf('Evolution of standing feedback circuit\np1=%4.2f p2=%4.2f',p1,p2) );
end

function acNet = ac1Test_StandingFeedbackEvolutionParameterGrid()
    for i=1:8
        p1 = linspace( 0.01, 0.3, 8 ); p1 = p1(i);
        for j=1:8
            p2 = linspace( 0.01, 0.5, 8 ); p2 = p2(j);

            subplot( 8, 8, (i-1)*8+j );
            ac1Test_StandingFeedbackEvolution( p1, p2 )
            
            drawnow();
        end
    end
end
%}
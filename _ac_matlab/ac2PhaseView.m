function ac2PhaseView( acNet, spaceCoord, whichNodeX, whichNodeY, range )
%{
NOT YET IMPLEMENTED

% PHASE PLOT
    standTrace = ac1ExtractReagent( acNet, whichNodeX, false );
    tiredTrace = ac1ExtractReagent( acNet, whichNodeY, false );

    for i=1:size(spaceCoord,2)
        space = spaceCoord(1,i);
        numPoints = 3*floor( acNet.T(end) ); % sample one point per second
        st(i,:) = interp1( acNet.T, standTrace(:,space), linspace(0,acNet.T(end),numPoints), 'spline' );
        tt(i,:) = interp1( acNet.T, tiredTrace(:,space), linspace(0,acNet.T(end),numPoints), 'spline' );
    end
    plot( st', tt', '-o', 'MarkerSize', 3 );
    
    axis( range );
    xlabel( '<--- sitting           standing --->' );
    ylabel( '<--- excitable         tired --->' );

    hold on;
    plot( st(:,1), tt(:,1), 'k*', 'MarkerSize', 10 );
    hold off;
%}    
end


function mov = ac2View( acNet, which )
    % SPLICE out a view of all space for one reagent
    View = ac2ExtractReagent( acNet, which, false );

    % @TODO: interpolate since T is not constant intervals
    fig = figure(1);
    numFrames = size(View,1);
    winSize = get( fig, 'Position' );
    winSize(1:2) = [ 0 0 ];
    mov = moviein( numFrames, fig, winSize );
    set( fig, 'NextPlot', 'replacechildren' );

    for t=1:numFrames
        V = reshape( View(t,:), acNet.dim, acNet.dim );
        imagesc( V );
        caxis( [ 0 1 ] );
        mov(:,t) = getframe( fig, winSize );
    end
    
end

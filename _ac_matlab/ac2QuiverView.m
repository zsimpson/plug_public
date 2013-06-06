function ac1QuiverView( acNet, arrowCount, arrowScale, view )
%{
    NOT YET IMPLMENTED 
    % MESHGRID the area that will be used for the quiver plot
    lft = view(1);
    rgt = view(2);
    bot = view(3);
    top = view(4);
    [X,Y] = meshgrid( lft:(rgt-lft)/arrowCount:rgt, bot:(top-bot)/arrowCount:top );
    DX = zeros( size(X) );
    DY = zeros( size(Y) );

	% COMPUTE the function value at each coordinate
    % in the the meshgrid so that it will be in the
    % format that quiver wants to draw the arrows
	[sx,sy] = size(X);
    for i=1:sx
        for j=1:sy
            delta = acNet.dyFunc( acNet, [ X(i,j); Y(i,j) ] );
            DX(i,j) = delta(1);
            DY(i,j) = delta(2);
        end
    end

    quiver(X,Y,DX,DY,arrowScale,'Color',[0.6 0.6 0.6]);
    axis( [lft rgt bot top] );
%}
end

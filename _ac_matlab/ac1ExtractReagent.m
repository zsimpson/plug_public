function Y = ac1ExtractReagent( acNet, which, withExtraCol )
    % EXTRACTS a given reagents all all positions in space
    % So the returned matrix has time count rows and xDim cols
    Y = acNet.Y( :, [which:acNet.nodeCount:acNet.xDim*acNet.nodeCount] );
    if( withExtraCol )
        % When viewing, the extra column makes it so that the pcolor
        % will correctly draw the last column
        Y(:,size(Y,2)+1) = zeros(size(Y,1),1);
    end
end


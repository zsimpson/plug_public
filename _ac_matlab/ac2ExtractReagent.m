function Y = ac2ExtractReagent( acNet, which, withExtraCol )
    % EXTRACTS a given reagents at all positions in space
    % So the returned matrix has time count rows and dim*dim cols
    Y = acNet.Y( :, [which:acNet.nodeCount:acNet.dim*acNet.dim*acNet.nodeCount] );
    if( withExtraCol )
        % When viewing, the extra column makes it so that the pcolor
        % will correctly draw the last column
        Y(:,size(Y,2)+1) = zeros(size(Y,1),1);
    end
end


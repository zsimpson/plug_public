function acNet = ac2CreateICs( acNet, method, parameter1, parameter2, parameter3, parameter4 )
    nc = acNet.nodeCount;
    dim = acNet.dim;
    for i=1:nc
        ICs = zeros( dim, dim );
        switch method
            case 'add-mat',
                % parameter1 is which node to add to
                % parameter2 is mat
                if i == parameter1                 
                    ICs = acNet.ICs{i};
                    ICs = ICs + parameter2;
                end
            case 'add-spot',
                % parameter1 is which node to add to
                % parameter2 is how much
                % parameter3,parameter4 are x,y
                if i == parameter1
                    ICs = acNet.ICs{i};
                    ICs(parameter3,parameter4) = ICs(parameter3,parameter4) + parameter2;
                end
        end
        acNet.ICs{i} = ICs;
    end
end


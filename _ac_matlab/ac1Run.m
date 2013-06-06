function acNet = ac1Run( acNet, duration )
    icRowVec = reshape( acNet.ICs, 1, size(acNet.ICs,1)*size(acNet.ICs,2) );
    [acNet.T,acNet.Y] = ode45( @ac1DY, [0 duration], icRowVec, [], acNet );
end


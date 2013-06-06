function acNet = ac2DoubleSpatialRes( acNet )
%{
NOT YET IMPLEMENTED

a = acNet;
    a.dim = a.dim * 2;
    a.capacitance = a.capacitance / 2;
    a.diff = a.diff * 2;
    a.gateConc = a.gateConc / 2;
    for x=1:a.xDim
        a.ICs(:,x) = acNet.ICs( :, floor((x-1)/2)+1 );
    end
    acNet = a;
%}
end

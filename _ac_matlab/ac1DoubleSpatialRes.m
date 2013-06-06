function acNet = ac1DoubleSpatialRes( acNet )
    a = acNet;
    a.xDim = a.xDim * 2;
    a.capacitance = a.capacitance / 2;
    a.diff = a.diff * 2;
    a.gateConc = a.gateConc / 2;
    for x=1:a.xDim
        a.ICs(:,x) = acNet.ICs( :, floor((x-1)/2)+1 );
    end
    acNet = a;
end

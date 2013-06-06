function ac1View( acNet, method, which )
    % SETUP the grid of verts for plotting
    [XGrid, YGrid] = meshgrid( 1:acNet.xDim+1, acNet.T );

    switch method
        case 'one-nosubplot',
            % SPLICE out a view of all space for one reagent
            View = ac1ExtractReagent( acNet, which, true );
            % CREATE plot
            pcolor( XGrid, YGrid, View );
            shading flat;
            caxis( [-1 1] );
            axis off;
        case 'all',
            % DRAW all of the nodes
            count = acNet.nodeCount;
            for( i=1:count )
                % SPLICE out a view of all space for one reagent
                View = ac1ExtractReagent( acNet, i, true );
                % CREATE plot
                subplot( 1, count, i );
                pcolor( XGrid, YGrid, View );
                shading flat;
                caxis( [-1 1] );
                axis off;
            end
        case 'some',
            % DRAW all of the inputs in the which vector
            count = size(which,2);
            for( i=1:count )
                % SPLICE out a view of all space for one reagent
                View = ac1ExtractReagent( acNet, which(1,i), true );
                % CREATE plot
                subplot( 1, count, i );
                pcolor( XGrid, YGrid, View );
                shading flat;
                caxis( [-1 1] );
                axis off;
            end
        case 'lines',
            % DRAW overlayed cross sections through all nodes
            count = acNet.nodeCount;
            figure1 = figure('Color',[1 1 1]);
            axes('FontSize',14,'Color',[1 1 1]);
            for( i=1:count )
                % SPLICE out a view of one reagent
                View = ac1ExtractReagent( acNet, i, true );
                % overlay plots of each reagent through a line at 'which'
                figure1=plot(View(which,:),'LineWidth',2,'Color',hsv2rgb([abs(sin(1+.7*i)), 1, 1])); 
                hold on;
            end
            xlim([0 length(View(which,:))]);
       case 'lines-SpecificAssociation',
            % DRAW overlayed cross sections through all nodes
            count = acNet.nodeCount;
            figure1 = figure('Color',[1 1 1]);
            axes('FontSize',14,'Color',[1 1 1]);

                % SPLICE out a view of one reagent
                View = ac1ExtractReagent( acNet, 1, true );
                % overlay plots of each reagent through a line at 'which'
                figure1=plot(View(which,:),'LineWidth',2,'Color',hsv2rgb([1, 1, 1])); 
                hold on;
                View = ac1ExtractReagent( acNet, 2, true ) + ac1ExtractReagent( acNet, 4, true );
                % overlay plots of each reagent through a line at 'which'
                figure1=plot(View(which,:),'LineWidth',2,'Color',hsv2rgb([.7, 1, 1])); 
                
                xlim([0 length(View(which,:))]);

    end
end

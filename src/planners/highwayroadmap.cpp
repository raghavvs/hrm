#include <iostream>
#include <list>
#include <src/planners/highwayroadmap.h>
#include<src/planners/interval.h>

#include <fstream>

#define pi 3.1415926

highwayRoadmap::highwayRoadmap(SuperEllipse robot, double endpt[2][2], SuperEllipse* arena, SuperEllipse* obs, option opt){
    Robot = robot;
    Endpt = *endpt;
    Arena = arena;
    Obs = obs;
    N_o = opt.N_o;
    N_s = opt.N_s;

    infla = opt.infla;
    N_layers = opt.N_layers;
    N_dy = opt.N_dy;
    N_KCsample = opt.sampleNum;

    Cost = 0;
}

void highwayRoadmap::buildRoadmap(){
    // angle steps
    double dr = pi/(N_layers-1);
    graph multiGraph;

    for(int i=0; i<N_layers; i++){
        Robot.a[2] = dr*i;
        // boundary for obstacles and arenas
        boundary bd = boundaryGen();

        // collision-free cells, stored by ty, xL, xU, xM
        cf_cell CFcell = rasterScan(bd.bd_s, bd.bd_o);

        // construct adjacency matrix for one layer
        connectOneLayer(CFcell);

        // Store the number of vertex before the current layer
        N_v_layer.push_back(vtxEdge.vertex.size());
    }
    connectMultiLayer();
}

boundary highwayRoadmap::boundaryGen(){
    SuperEllipse robot_infla = Robot;
    boundary bd;

    // Enlarge the robot
    robot_infla.a[0] *= 1+infla;
    robot_infla.a[1] *= 1+infla;

    // calculate Minkowski boundary points
    for(int i=0; i<N_s; i++)
        bd.bd_s.push_back( Arena[i].minkSum2D(Arena[i].a, robot_infla.a, Arena[i].num, -1) );
    for(int i=0; i<N_o; i++)
        bd.bd_o.push_back( Obs[i].minkSum2D(Obs[i].a, robot_infla.a, Obs[i].num, +1) );

    return bd;
}

cf_cell highwayRoadmap::rasterScan(vector<MatrixXd> bd_s, vector<MatrixXd> bd_o){
    cf_cell cell, cell_new;

    boundary::sepBd P_bd_s[N_s], P_bd_o[N_o];
    boundary::sepBd x_bd_s[N_dy][N_s], x_bd_o[N_dy][N_o];

    MatrixXd bd_s_L[N_s], bd_s_R[N_s], bd_o_L[N_o], bd_o_R[N_o];
    MatrixXd x_s_L(N_dy, N_s), x_s_R(N_dy, N_s);
    MatrixXd x_o_L(N_dy, N_o), x_o_R(N_dy, N_o);

    interval op;
    vector<Interval> cf_seg[N_dy], obs_seg, arena_seg, obs_merge, arena_inter;
    vector<double> xL, xU, xM;

    // Separate boundaries of Arenas and Obstacles into two parts
    for(int i=0; i<N_s; i++) {
        P_bd_s[i] = separateBoundary(bd_s[i]);
        bd_s_L[i] = P_bd_s[i].P_bd_L;
        bd_s_R[i] = P_bd_s[i].P_bd_R;
    }
    for(int i=0; i<N_o; i++) {
        P_bd_o[i] = separateBoundary(bd_o[i]);
        bd_o_L[i] = P_bd_o[i].P_bd_L;
        bd_o_R[i] = P_bd_o[i].P_bd_R;
    }

    // Find closest points for each raster scan line
    double ty[N_dy], dy = (P_bd_s[0].max_y-P_bd_s[0].min_y)/(N_dy-1);
    for(int i=0; i<N_dy; i++){
        // y-coordinate of each sweep line
        ty[i] = P_bd_s[0].min_y + i * dy;
        // x-coordinate of the intersection btw sweep line and arenas
        for(int j=0; j<N_s; j++) {
            x_bd_s[i][j] = closestPt(P_bd_s[j], ty[i]);
            x_s_L(i,j) = x_bd_s[i][j].x_L;
            x_s_R(i,j) = x_bd_s[i][j].x_R;
        }
        // x-coordinate of the intersection btw sweep line and obstacles
        for(int j=0; j<N_o; j++) {
            x_bd_o[i][j] = closestPt(P_bd_o[j], ty[i]);
            x_o_L(i,j) = x_bd_o[i][j].x_L;
            x_o_R(i,j) = x_bd_o[i][j].x_R;
        }
    }

//    ofstream file_obs;
//    file_obs.open("bd_obs.csv");
//    file_obs << x_o_L << "\n";
//    file_obs << x_o_R << "\n";
//    file_obs.close();

//    // Enlarge the obstacle to form convex CF cells
//    x_o_L = boundaryEnlarge(bd_o_L, x_o_L, ty, -1);
//    x_o_R = boundaryEnlarge(bd_o_R, x_o_R, ty, +1);

//    // write to .csv file
//    ofstream file_ty;
//    file_ty.open("bd_ty.csv");
//    for(int i=0; i<N_dy; i++) file_ty << ty[i] << "\n";
//    file_ty.close();

//    file_obs.open("bd_obs_ex.csv");
//    file_obs << x_o_L << "\n";
//    file_obs << x_o_R << "\n";
//    file_obs.close();

//    ofstream file_arena;
//    file_arena.open("bd_arena.csv");
//    file_arena << x_s_L << "\n";
//    file_arena << x_s_R << "\n";
//    file_arena.close();

    // CF line segment for each ty
    for(int i=0; i<N_dy; i++){
        // Construct intervals at each sweep line
        for(int j=0; j<N_s; j++) if(!isnan(x_s_L(i,j)) && !isnan(x_s_R(i,j)))
            arena_seg.push_back({x_s_L(i,j), x_s_R(i,j)});
        for(int j=0; j<N_o; j++) if(!isnan(x_o_L(i,j)) && !isnan(x_o_R(i,j)))
            obs_seg.push_back({x_o_L(i,j), x_o_R(i,j)});

        // y-coord
        cell.ty.push_back(ty[i]);

        // cf-intervals at each line
        obs_merge = op.Union(obs_seg);
        arena_inter = op.Intersect(arena_seg);
        cf_seg[i] = op.Complement(arena_inter,obs_merge);

        // x-coords
        for(int j=0; j<cf_seg[i].size(); j++){
            xL.push_back(cf_seg[i][j].s);
            xU.push_back(cf_seg[i][j].e);
            xM.push_back( (cf_seg[i][j].s+cf_seg[i][j].e)/2.0 );
        }
        cell.xL.push_back(xL);
        cell.xU.push_back(xU);
        cell.xM.push_back(xM);

        // Clear memory
        arena_seg.clear();
        obs_seg.clear();
        xL.clear();
        xU.clear();
        xM.clear();
    }

    // Enhanced cell decomposition
    cell_new = enhanceDecomp(cell);

    return cell_new;
}

void highwayRoadmap::connectOneLayer(cf_cell CFcell){
    // Construct a vector of vertex
    for(int i=0; i<CFcell.ty.size(); i++){
        int N_0 = vtxEdge.vertex.size();
        for(int j=0; j<CFcell.xM[i].size(); j++){
            vtxEdge.vertex.push_back({CFcell.xM[i][j], CFcell.ty[i], Robot.a[2]});

            // Connect vertex within one sweep line
            if(j != CFcell.xM[i].size()-1){
                if(abs(CFcell.xU[i][j] - CFcell.xL[i][j+1]) < 1e-5){
                    vtxEdge.edge.push_back(make_pair(N_0+j, N_0+j+1));
                    vtxEdge.weight.push_back(1.0);
                }
            }
        }
        int N_1 = vtxEdge.vertex.size();

        if(i != CFcell.ty.size()-1){
        // Connect vertex btw adjacent cells
            for(int j1=0; j1<CFcell.xM[i].size(); j1++){
                for(int j2=0; j2<CFcell.xM[i+1].size(); j2++){
                    if( ( (CFcell.xM[i][j1] >= CFcell.xL[i+1][j2] && CFcell.xM[i][j1] <= CFcell.xU[i+1][j2]) ||
                        (CFcell.xM[i+1][j2] >= CFcell.xL[i][j1] && CFcell.xM[i+1][j2] <= CFcell.xU[i][j1]) ) &&
                        ( (CFcell.xU[i][j1] >= CFcell.xL[i+1][j2] && CFcell.xU[i][j1] <= CFcell.xU[i+1][j2]) ||
                        (CFcell.xL[i][j1] >= CFcell.xL[i+1][j2] && CFcell.xL[i][j1] <= CFcell.xU[i+1][j2]) ||
                        (CFcell.xU[i+1][j2] >= CFcell.xL[i][j1] && CFcell.xU[i+1][j2] <= CFcell.xU[i][j1]) ||
                        (CFcell.xL[i+1][j2] >= CFcell.xL[i][j1] && CFcell.xL[i+1][j2] <= CFcell.xU[i][j1]) ) ){
                            vtxEdge.edge.push_back(make_pair(N_0+j1, N_1+j2));
                            vtxEdge.weight.push_back(2.0);
                    }
                }
            }
        }
    }
}

void highwayRoadmap::connectMultiLayer(){
    int n_1, n_2, d;
    int start = 0;

    for(int i=0; i<N_layers-1; i++){
        // Find vertex only in adjecent layers
        n_1 = N_v_layer[i];
        if(i==N_layers) n_2 = N_v_layer[0];
            else n_2 = N_v_layer[i+1];

        // Nearest vertex btw layers
        for(int m=start; m<n_1; m++){
            for(int m2=n_1; m2<n_2; m2++){
                d = pow((vtxEdge.vertex[m][0]-vtxEdge.vertex[m2][0]),2.0)+
                        pow((vtxEdge.vertex[m][1]-vtxEdge.vertex[m2][1]),2.0);
                if(d < 1.0){
                    vtxEdge.edge.push_back(make_pair(m, m2));
                    vtxEdge.weight.push_back(3.0);
                    break;
                }
            }
        }
        start = n_1;
    }
}

void highwayRoadmap::search(){
    // Construct the roadmap
    int num_vtx = vtxEdge.vertex.size();
    AdjGraph g(num_vtx);

    for(int i=0; i<vtxEdge.edge.size(); i++)
        add_edge(vtxEdge.edge[i].first, vtxEdge.edge[i].second, g);

    // Define start and goal
    mt19937 gen(time(0));
    vertex_descriptor start = random_vertex(g, gen);
    vertex_descriptor goal = random_vertex(g, gen);

    // Search for shortest path
    astar_search(g, start,
       AStarHeuristic h, const bgl_named_params<P, T, R>& params);
}


/************************************************************************************************/
/*********************************** Private Functions ******************************************/
/************************************************************************************************/
// For a given curve, separate its boundary into two parts
boundary::sepBd highwayRoadmap::separateBoundary(MatrixXd bd){
    const int half_num = Arena[0].num/2;
    MatrixXd::Index I_max_y, I_min_y;
    int I_start_y;
    MatrixXd P_bd_L, P_bd_R;
    double max_y, min_y;
    boundary::sepBd P_bd;

    // Find separating point
    max_y = bd.row(1).maxCoeff(&I_max_y);
    min_y = bd.row(1).minCoeff(&I_min_y);
    I_start_y = min(I_max_y, I_min_y);
    I_start_y = min(I_start_y, half_num);

    // Left part
    P_bd_L.setZero(2,half_num);
    P_bd_L = bd.block(0, I_start_y, 2, half_num);
    // Right part
    P_bd_R.setZero(2,half_num);
    P_bd_R.topLeftCorner(2, I_start_y) = bd.topLeftCorner(2, I_start_y);
    P_bd_R.bottomRightCorner(2, half_num-I_start_y) = bd.bottomRightCorner(2, half_num-I_start_y);

    P_bd.P_bd_L = P_bd_L;
    P_bd.P_bd_R = P_bd_R;
    P_bd.max_y = max_y;
    P_bd.min_y = min_y;

    return P_bd;
}

boundary::sepBd highwayRoadmap::closestPt(boundary::sepBd P_bd, double ty){
    boundary::sepBd x_bd;
    MatrixXd::Index I_L, I_R;
    VectorXd y(1);

    // check if ty in the range of each arena/obstacle
    if( (ty > P_bd.max_y) || (ty < P_bd.min_y) ){
        x_bd.x_L = numeric_limits<double>::quiet_NaN();
        x_bd.x_R = numeric_limits<double>::quiet_NaN();
        return x_bd;
    }
    // For each ty, find closes point, ie the intersection btw sweep line and arena/obstacle
    y << ty;
    (P_bd.P_bd_L.row(1).colwise() - y).colwise().squaredNorm().minCoeff(&I_L);
    (P_bd.P_bd_R.row(1).colwise() - y).colwise().squaredNorm().minCoeff(&I_R);

    x_bd.x_L = P_bd.P_bd_L(0,I_L);
    x_bd.x_R = P_bd.P_bd_R(0,I_R);
    return x_bd;
}

MatrixXd highwayRoadmap::boundaryEnlarge(MatrixXd bd_o[], MatrixXd x_o, double ty[], int K){
    MatrixXd x_o_Ex(N_dy, N_o);
    double x_Ex;
    double d;

    // Initialize x_o_Ex as NaN matrix
    for(int j=0; j<N_o; j++){
        for(int i=0; i<N_dy; i++){
            x_o_Ex(i,j) = numeric_limits<double>::quiet_NaN();
        }
    }

    // Compute tangent line and enlarge the boundary
    for(int j=0; j<N_o; j++){
        int count = 0;

        for(int i=0; i<N_dy; i++){
            double dist = 0, phi;

            if( isnan(x_o(i,j)) || isnan(x_o(i+1,j)) ) continue;
            count += 1;
            double p1[2] = {x_o(i,j), ty[i]};
            double p2[2] = {x_o(i+1,j), ty[i+1]};

            // Search for farthest point to the line segment def by two intersecting points
            for(int k=0; k<sizeof(bd_o[j]); k++){
                double p[2] = {bd_o[j](0,k), bd_o[j](1,k)};

                if( (p[1] > ty[i]) && (p[1] < ty[i+1]) ){
                    d = abs( (p2[1]-p1[1])*p[0] - (p2[0]-p1[0])*p[1] + p2[0]*p1[1] - p2[1]*p1[0] )
                            / sqrt( pow((p2[1]-p1[1]),2) + pow((p2[0]-p1[0]),2) );
                    if(d > dist) dist = d;
                }
            }
            phi = atan2( p2[1]-p1[1], p2[0]-p1[0] );
            x_Ex = x_o(i,j) + K * dist/sin(phi);

            // Update the boundary point to be farthest to the previous one
            if(K == -1) {if(x_Ex <= x_o(i,j)) x_o_Ex(i,j) = x_Ex;}
            else if(K == +1) {if(x_Ex >= x_o(i,j)) x_o_Ex(i,j) = x_Ex;}

            if(count == 1) x_o_Ex(i,j) = x_o(i,j) + K * dist/sin(phi);
            x_o_Ex(i+1,j) = x_o(i+1,j) + K * dist/sin(phi);
        }
    }

    return x_o_Ex;
}

cf_cell highwayRoadmap::enhanceDecomp(cf_cell cell){
    cf_cell cell_new = cell;
    double ep = 0;

    for(int i=0; i<cell.ty.size()-1; i++){
        for(int j1=0; j1<cell.xM[i].size(); j1++){
            for(int j2=0; j2<cell.xM[i+1].size(); j2++){
                if(cell_new.xM[i][j1] < cell_new.xL[i+1][j2] && cell_new.xU[i][j1] >= cell_new.xL[i+1][j2]){
                    cell_new.xU[i].push_back(cell_new.xL[i+1][j2]);
                    cell_new.xL[i].push_back(cell_new.xL[i+1][j2]);
                    cell_new.xM[i].push_back(cell_new.xL[i+1][j2]);
                }
                else if(cell_new.xM[i][j1] > cell_new.xU[i+1][j2] && cell_new.xL[i][j1] <= cell_new.xU[i+1][j2]){
                    cell_new.xU[i].push_back(cell_new.xU[i+1][j2]);
                    cell_new.xL[i].push_back(cell_new.xU[i+1][j2]);
                    cell_new.xM[i].push_back(cell_new.xU[i+1][j2]);
                }

                if(cell_new.xM[i+1][j2] < cell_new.xL[i][j1] && cell_new.xU[i+1][j2] >= cell_new.xL[i][j1]){
                    cell_new.xU[i+1].push_back(cell_new.xL[i][j1]);
                    cell_new.xL[i+1].push_back(cell_new.xL[i][j1]);
                    cell_new.xM[i+1].push_back(cell_new.xL[i][j1]);
                }
                else if(cell_new.xM[i+1][j2] > cell_new.xU[i][j1] && cell_new.xL[i+1][j2] <= cell_new.xU[i][j1]){
                    cell_new.xU[i+1].push_back(cell_new.xU[i][j1]);
                    cell_new.xL[i+1].push_back(cell_new.xU[i][j1]);
                    cell_new.xM[i+1].push_back(cell_new.xU[i][j1]);
                }
            }
        }

        sort(cell_new.xL[i].begin(), cell_new.xL[i].end(), [](double a, double b){return a < b;});
        sort(cell_new.xU[i].begin(), cell_new.xU[i].end(), [](double a, double b){return a < b;});
        sort(cell_new.xM[i].begin(), cell_new.xM[i].end(), [](double a, double b){return a < b;});
    }

    return cell_new;
}

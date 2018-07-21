#include <iostream>
#include <fstream>
#include <chrono>

#include <math.h>
#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Geometry>
#include <vector>

#include <src/geometry/superellipse.h>
#include <src/planners/highwayroadmap.h>

using namespace Eigen;
using namespace std;

#define pi 3.1415926

vector<vector<double>> parse2DCsvFile(string inputFileName) {
    vector<vector<double> > data;
    ifstream inputFile(inputFileName);
    int l = 0;

    while (inputFile) {
        l++;
        string s;
        if (!getline(inputFile, s)) break;
        if (s[0] != '#') {
            istringstream ss(s);
            vector<double> record;

            while (ss) {
                string line;
                if (!getline(ss, line, ','))
                    break;
                try {
                    record.push_back(stof(line));
                }
                catch (const std::invalid_argument e) {
                    cout << "NaN found in file " << inputFileName << " line " << l
                         << endl;
                    e.what();
                }
            }

            data.push_back(record);
        }
    }

    if (!inputFile.eof()) {
        cerr << "Could not read file " << inputFileName << "\n";
        __throw_invalid_argument("File not found.");
    }

    return data;
}

highwayRoadmap plan(unsigned int N_l, unsigned int N_y){
    // Number of points on the boundary
    int num = 50;

    // Robot
    // Read robot config file
    string file_robConfig = "../config/robotConfig.csv";
    vector<vector<double> > rob_config = parse2DCsvFile(file_robConfig);

    string file_robVtx = "../config/robotVtx.csv";
    vector<vector<double> > rob_vtx = parse2DCsvFile(file_robVtx);

    string file_robInvMat = "../config/robotInvMat.csv";
    vector<vector<double> > rob_InvMat = parse2DCsvFile(file_robInvMat);

    // Robot as a class of SuperEllipse
    vector<SuperEllipse> robot(rob_config.size());
    for(size_t j=0; j<rob_config.size(); j++){
        for(size_t i=0; i<6; i++) robot[j].a[i] = rob_config[0][i];
        robot[j].num = num;
    }

    polyCSpace polyVtx;
    polyVtx.vertex = rob_vtx;
    polyVtx.invMat = rob_InvMat;

    // Environment
    // Read environment config file
    string file_arenaConfig = "../config/arenaConfig.csv";
    vector<vector<double> > arena_config = parse2DCsvFile(file_arenaConfig);

    string file_obsConfig = "../config/obsConfig.csv";
    vector<vector<double> > obs_config = parse2DCsvFile(file_obsConfig);

    string file_endpt = "../config/endPts.csv";
    vector<vector<double> > endPts = parse2DCsvFile(file_endpt);
    vector< vector<double> > EndPts;

    EndPts.push_back(endPts[1]);
    EndPts.push_back(endPts[2]);

    // Arena and Obstacles as class of SuperEllipse
    vector<SuperEllipse> arena(arena_config.size()), obs(obs_config.size());
    for(size_t j=0; j<arena_config.size(); j++){
        for(size_t i=0; i<6; i++) arena[j].a[i] = arena_config[j][i];
        arena[j].num = num;
    }
    for(size_t j=0; j<obs_config.size(); j++){
        for(size_t i=0; i<6; i++) obs[j].a[i] = obs_config[j][i];
        obs[j].num = num;
    }

    // Options
    option opt;
    opt.infla = rob_config[0][6];
    opt.N_layers = N_l;
    opt.N_dy = N_y;
    opt.sampleNum = 10;

    opt.N_o = obs.size();
    opt.N_s = arena.size();

    //****************//
    // Main Algorithm //
    //****************//
    highwayRoadmap high(robot, polyVtx, EndPts, arena, obs, opt);
    high.plan();

    // calculate original boundary points
    boundary bd_ori;
    for(size_t i=0; i<opt.N_s; i++){
        bd_ori.bd_s.push_back( high.Arena[i].originShape(high.Arena[i].a, high.Arena[i].num) );
    }
    for(size_t i=0; i<opt.N_o; i++){
        bd_ori.bd_o.push_back( high.Obs[i].originShape(high.Obs[i].a, high.Obs[i].num) );
    }

    // Output boundary and cell info
    boundary bd = high.boundaryGen();
    cf_cell cell = high.rasterScan(bd.bd_s, bd.bd_o);
    high.connectOneLayer(cell);


    // write to .csv file
    ofstream file_ori_bd;
    file_ori_bd.open("bd_ori.csv");
    for(size_t i=0; i<bd_ori.bd_o.size(); i++) file_ori_bd << bd_ori.bd_o[i] << "\n";
    for(size_t i=0; i<bd_ori.bd_s.size(); i++) file_ori_bd << bd_ori.bd_s[i] << "\n";
    file_ori_bd.close();

    ofstream file_bd;
    file_bd.open("bd.csv");
    for(size_t i=0; i<bd.bd_o.size(); i++) file_bd << bd.bd_o[i] << "\n";
    for(size_t i=0; i<bd.bd_s.size(); i++) file_bd << bd.bd_s[i] << "\n";
    file_bd.close();

    ofstream file_cell;
    file_cell.open("cell.csv");
    for(size_t i=0; i<cell.ty.size(); i++){
        for(size_t j=0; j<cell.xL[i].size(); j++)
            file_cell << cell.ty[i] << ' ' <<
                         cell.xL[i][j] << ' ' <<
                         cell.xM[i][j] << ' ' <<
                         cell.xU[i][j] << "\n";
    }
    file_cell.close();

    return high;
}

int main(int argc, char ** argv){
    if (argc != 4) {
        cerr<< "Usage: Please add 1) Num of trials 2) Num of layers 3) Num of sweep lines" << endl;
        return 1;
    }

    // Record planning time for N trials
    int N = atoi(argv[1]), N_l = atoi(argv[2]), N_y = atoi(argv[3]);
    vector<double> time_stat[N];
    highwayRoadmap high = plan(N_l, N_y);

    for(int i=0; i<N; i++){
        highwayRoadmap high = plan(N_l, N_y);
        time_stat[i].push_back(high.planTime.buildTime);
        time_stat[i].push_back(high.planTime.searchTime);
        time_stat[i].push_back(high.planTime.buildTime + high.planTime.searchTime);
        time_stat[i].push_back(high.vtxEdge.vertex.size());
        time_stat[i].push_back(high.vtxEdge.edge.size());
        time_stat[i].push_back(high.Paths.size());

        // Planning Time and Path Cost
        cout << "Roadmap build time: " << high.planTime.buildTime << "s" << endl;
        cout << "Path search time: " << high.planTime.searchTime << "s" << endl;
        cout << "Total Planning Time: " << high.planTime.buildTime + high.planTime.searchTime << 's' << endl;

        cout << "Number of valid configurations: " << high.vtxEdge.vertex.size() << endl;
        cout << "Number of configurations in Path: " << high.Paths.size() <<  endl;
        cout << "Cost: " << high.Cost << endl;
    }

    // Write the output to .csv files
    ofstream file_vtx;
    file_vtx.open("vertex.csv");
    vector<vector<double>> vtx = high.vtxEdge.vertex;
    for(size_t i=0; i<vtx.size(); i++) file_vtx << vtx[i][0] << ' ' << vtx[i][1] << ' ' << vtx[i][2] << "\n";
    file_vtx.close();

    ofstream file_edge;
    file_edge.open("edge.csv");
    vector<pair<int, int>> edge = high.vtxEdge.edge;
    for(size_t i=0; i<edge.size(); i++) file_edge << edge[i].first << ' ' << edge[i].second << "\n";
    file_edge.close();

    ofstream file_paths;
    file_paths.open("paths.csv");
    vector<int> paths = high.Paths;
    for(size_t i=0; i<paths.size(); i++) file_paths << paths[i] << ' ';
    file_paths.close();

    // Store results
    ofstream file_time;
    file_time.open("time_high.csv");
    file_time << "BUILD_TIME" << ',' << "SEARCH_TIME" << ',' << "PLAN_TIME" << ',' << "GRAPH_NODE" << ',' << "GRAPH_EDGE" << ',' << "PATH_NODE" << "\n";
    for(size_t i=0; i<N; i++) file_time << time_stat[i][0] << ',' << time_stat[i][1] << ',' << time_stat[i][2] << ','
              << time_stat[i][3] << ',' << time_stat[i][4] << ',' << time_stat[i][5] << "\n";
    file_time.close();

    return 0;
}

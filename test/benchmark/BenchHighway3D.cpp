#include "planners/include/HRM3DMultiBody.h"
#include "util/include/DisplayPlanningData.h"
#include "util/include/ParsePlanningSettings.h"

#include <stdlib.h>
#include <cstdlib>
#include <ctime>

using namespace Eigen;
using namespace std;

int main(int argc, char** argv) {
    if (argc < 5) {
        cerr << "Usage: Please add 1) Num of trials 2) Num of layers 3) Num of "
                "sweep planes 4) Num of sweep lines 5) Pre-defined quaternions "
                "file prefix (if no, enter 0 or leave blank)"
             << endl;
        return 1;
    }

    // Record planning time for N trials
    const size_t N = size_t(atoi(argv[1]));
    const int N_l = atoi(argv[2]);
    const int N_x = atoi(argv[3]);
    const int N_y = atoi(argv[4]);

    // Setup environment config
    PlannerSetting3D* env3D = new PlannerSetting3D();
    env3D->loadEnvironment();

    // Setup robot
    string quat_file = "0";
    if (argc == 6 && strcmp(argv[5], "0") != 0) {
        quat_file = string(argv[5]) + '_' + string(argv[2]) + ".csv";
    }
    MultiBodyTree3D robot =
        loadRobotMultiBody3D(quat_file, env3D->getNumSurfParam());

    // Options
    PlannerParameter par;
    par.NUM_LAYER = size_t(N_l);
    par.NUM_LINE_X = size_t(N_x);
    par.NUM_LINE_Y = size_t(N_y);

    double f = 1.5;
    par.BOUND_LIMIT = {env3D->getArena().at(0).getSemiAxis().at(0) -
                           f * robot.getBase().getSemiAxis().at(0),
                       env3D->getArena().at(0).getSemiAxis().at(1) -
                           f * robot.getBase().getSemiAxis().at(0),
                       env3D->getArena().at(0).getSemiAxis().at(2) -
                           f * robot.getBase().getSemiAxis().at(0)};

    PlanningRequest req;
    req.is_robot_rigid = true;
    req.planner_parameters = par;
    req.start = env3D->getEndPoints().at(0);
    req.goal = env3D->getEndPoints().at(1);

    // Store results
    ofstream file_time;
    file_time.open("time_high_3D.csv");
    file_time << "SUCCESS" << ',' << "BUILD_TIME" << ',' << "SEARCH_TIME" << ','
              << "PLAN_TIME" << ',' << "N_LAYERS" << ',' << "N_X" << ','
              << "N_Y" << ',' << "GRAPH_NODE" << ',' << "GRAPH_EDGE" << ','
              << "PATH_NODE"
              << "\n";

    for (size_t i = 0; i < N; i++) {
        cout << "Number of trials: " << i + 1 << endl;

        // Path planning using HRM3DMultiBody
        HRM3DMultiBody hrm(robot, env3D->getArena(), env3D->getObstacle(), req);
        hrm.plan();

        PlanningResult res = hrm.getPlanningResult();

        // Display and store results
        displayPlanningTimeInfo(&res.planning_time);
        displayGraphInfo(&res.graph_structure);
        displayPathInfo(&res.solution_path);

        file_time << res.solved << ',' << res.planning_time.buildTime << ','
                  << res.planning_time.searchTime << ','
                  << res.planning_time.totalTime << ',' << N_l << ',' << N_x
                  << ',' << N_y << ',' << res.graph_structure.vertex.size()
                  << ',' << res.graph_structure.edge.size() << ','
                  << res.solution_path.PathId.size() << "\n";
    }
    file_time.close();

    return 0;
}

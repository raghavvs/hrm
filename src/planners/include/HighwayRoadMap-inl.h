#ifndef HIGHWAYROADMAP_INL_H
#define HIGHWAYROADMAP_INL_H

#include "HighwayRoadMap.h"
#include "src/util/include/Interval.h"
#include "util/include/DistanceMetric.h"

#include <algorithm>
#include <chrono>
#include <list>
#include <random>

using Clock = std::chrono::high_resolution_clock;
using Durationd = std::chrono::duration<double>;

/** \brief visitor that terminates when we find the goal */
struct AStarFoundGoal {};

template <class Vertex>
class AStarGoalVisitor : public boost::default_astar_visitor {
  public:
    AStarGoalVisitor(Vertex goal) : goal_(goal) {}
    template <class Graph>
    void examine_vertex(Vertex u, Graph& g) {
        if (u == goal_) {
            throw AStarFoundGoal();
        }
    }

  private:
    Vertex goal_;
};

/** \class HighwayRoadMap superclass for HRM-based planners */
template <class RobotType, class ObjectType>
HighwayRoadMap<RobotType, ObjectType>::HighwayRoadMap(
    const RobotType& robot, const std::vector<ObjectType>& arena,
    const std::vector<ObjectType>& obs, const PlanningRequest& req)
    : robot_(robot),
      arena_(arena),
      obs_(obs),
      start_(req.start),
      goal_(req.goal),
      param_(req.planner_parameters),
      N_o(obs.size()),
      N_s(arena.size()) {}

template <class RobotType, class ObjectType>
HighwayRoadMap<RobotType, ObjectType>::~HighwayRoadMap() {}

template <class RobotType, class ObjectType>
void HighwayRoadMap<RobotType, ObjectType>::plan() {
    // Plan and timing
    auto start = Clock::now();
    buildRoadmap();
    res_.planning_time.buildTime = Durationd(Clock::now() - start).count();

    start = Clock::now();
    search();
    res_.planning_time.searchTime = Durationd(Clock::now() - start).count();

    res_.planning_time.totalTime =
        res_.planning_time.buildTime + res_.planning_time.searchTime;

    // Get solution path
    res_.solution_path.solvedPath = getSolutionPath();
}

template <class RobotType, class ObjectType>
void HighwayRoadMap<RobotType, ObjectType>::search() {
    std::vector<Vertex> idx_s;
    std::vector<Vertex> idx_g;
    size_t num;

    // Construct the roadmap
    size_t num_vtx = res_.graph_structure.vertex.size();
    AdjGraph g(num_vtx);

    for (size_t i = 0; i < res_.graph_structure.edge.size(); ++i) {
        boost::add_edge(size_t(res_.graph_structure.edge[i].first),
                        size_t(res_.graph_structure.edge[i].second),
                        Weight(res_.graph_structure.weight[i]), g);
    }

    // Locate the nearest vertex for start and goal in the roadmap
    idx_s = getNearestNeighborsOnGraph(start_, param_.NUM_SEARCH_NEIGHBOR,
                                       param_.SEARCH_RADIUS);
    idx_g = getNearestNeighborsOnGraph(goal_, param_.NUM_SEARCH_NEIGHBOR,
                                       param_.SEARCH_RADIUS);

    // Search for shortest path in the searching regions
    for (Vertex idxS : idx_s) {
        for (Vertex idxG : idx_g) {
            std::vector<Vertex> p(num_vertices(g));
            std::vector<double> d(num_vertices(g));

            try {
                boost::astar_search(
                    g, idxS,
                    [this, idxG](Vertex v) {
                        return vectorEuclidean(
                            res_.graph_structure.vertex[v],
                            res_.graph_structure.vertex[idxG]);
                    },
                    boost::predecessor_map(
                        boost::make_iterator_property_map(
                            p.begin(), get(boost::vertex_index, g)))
                        .distance_map(make_iterator_property_map(
                            d.begin(), get(boost::vertex_index, g)))
                        .visitor(AStarGoalVisitor<Vertex>(idxG)));
            } catch (AStarFoundGoal found) {
                // Record path and cost
                num = 0;
                res_.solution_path.cost = 0.0;
                res_.solution_path.PathId.push_back(int(idxG));
                while (res_.solution_path.PathId[num] != int(idxS) &&
                       num <= num_vtx) {
                    res_.solution_path.PathId.push_back(
                        int(p[size_t(res_.solution_path.PathId[num])]));
                    res_.solution_path.cost +=
                        res_.graph_structure
                            .weight[size_t(res_.solution_path.PathId[num])];
                    num++;
                }
                std::reverse(std::begin(res_.solution_path.PathId),
                             std::end(res_.solution_path.PathId));

                if (num == num_vtx + 1) {
                    res_.solution_path.PathId.clear();
                    res_.solution_path.cost = inf;
                } else {
                    res_.solved = true;
                    return;
                }
            }
        }
    }
}

template <class RobotType, class ObjectType>
Boundary HighwayRoadMap<RobotType, ObjectType>::boundaryGen() {
    Boundary bd;

    // Minkowski boundary points
    std::vector<Eigen::MatrixXd> bdAux;
    for (size_t i = 0; i < size_t(N_s); ++i) {
        bdAux = robot_.minkSum(&arena_.at(i), -1);
        for (size_t j = 0; j < bdAux.size(); ++j) {
            bd.arena.push_back(bdAux.at(j));
        }
        bdAux.clear();
    }
    for (size_t i = 0; i < size_t(N_o); ++i) {
        bdAux = robot_.minkSum(&obs_.at(i), 1);
        for (size_t j = 0; j < bdAux.size(); ++j) {
            bd.obstacle.push_back(bdAux.at(j));
        }
        bdAux.clear();
    }

    return bd;
}

template <class RobotType, class ObjectType>
void HighwayRoadMap<RobotType, ObjectType>::connectOneLayer2D(
    const FreeSegment2D* freeSeg) {
    // Add connections to edge list
    size_t n1 = 0;
    size_t n2 = 0;

    for (size_t i = 0; i < freeSeg->ty.size(); ++i) {
        n1 = N_v.plane.at(i);

        for (size_t j1 = 0; j1 < freeSeg->xM[i].size(); ++j1) {
            // Connect vertex within the same sweep line
            if (j1 != freeSeg->xM[i].size() - 1) {
                if (std::fabs(freeSeg->xU[i][j1] - freeSeg->xL[i][j1 + 1]) <
                    1e-5) {
                    res_.graph_structure.edge.push_back(
                        std::make_pair(n1 + j1, n1 + j1 + 1));
                    res_.graph_structure.weight.push_back(vectorEuclidean(
                        res_.graph_structure.vertex[n1 + j1],
                        res_.graph_structure.vertex[n1 + j1 + 1]));
                }
            }

            // Connect vertex btw adjacent sweep lines
            if (i != freeSeg->ty.size() - 1) {
                n2 = N_v.plane.at(i + 1);

                for (size_t j2 = 0; j2 < freeSeg->xM[i + 1].size(); ++j2) {
                    if (isSameLayerTransitionFree(
                            res_.graph_structure.vertex[n1 + j1],
                            res_.graph_structure.vertex[n2 + j2])) {
                        res_.graph_structure.edge.push_back(
                            std::make_pair(n1 + j1, n2 + j2));
                        res_.graph_structure.weight.push_back(vectorEuclidean(
                            res_.graph_structure.vertex[n1 + j1],
                            res_.graph_structure.vertex[n2 + j2]));
                    }
                }
            }
        }
    }
}

template <class RobotType, class ObjectType>
std::vector<std::vector<double>>
HighwayRoadMap<RobotType, ObjectType>::getSolutionPath() {
    std::vector<std::vector<double>> path;
    auto poseSize = res_.graph_structure.vertex.at(0).size();

    // Start pose
    start_.resize(poseSize);
    path.push_back(start_);

    // Iteratively store intermediate poses along the solved path
    for (auto pathId : res_.solution_path.PathId) {
        path.push_back(res_.graph_structure.vertex.at(size_t(pathId)));
    }

    // Goal pose
    goal_.resize(poseSize);
    path.push_back(goal_);

    return path;
}

template <class RobotType, class ObjectType>
FreeSegment2D HighwayRoadMap<RobotType, ObjectType>::computeFreeSegment(
    const std::vector<double>& ty, const Eigen::MatrixXd& x_s_L,
    const Eigen::MatrixXd& x_s_U, const Eigen::MatrixXd& x_o_L,
    const Eigen::MatrixXd& x_o_U) {
    FreeSegment2D freeLineSegment;

    Interval op;
    std::vector<Interval> interval[param_.NUM_LINE_Y];
    std::vector<Interval> obsSeg;
    std::vector<Interval> arenaSeg;
    std::vector<Interval> obsMerge;
    std::vector<Interval> arenaIntersect;
    std::vector<double> xL;
    std::vector<double> xU;
    std::vector<double> xM;
    long N_cs = x_s_L.cols();
    long N_co = x_o_L.cols();

    // CF line segment for each ty
    for (size_t i = 0; i < param_.NUM_LINE_Y; ++i) {
        // Construct intervals at each sweep line
        for (auto j = 0; j < N_cs; ++j)
            if (!std::isnan(x_s_L(long(i), j)) &&
                !std::isnan(x_s_U(long(i), j))) {
                arenaSeg.push_back({x_s_L(long(i), j), x_s_U(long(i), j)});
            }
        for (auto j = 0; j < N_co; ++j)
            if (!std::isnan(x_o_L(long(i), j)) &&
                !std::isnan(x_o_U(long(i), j))) {
                obsSeg.push_back({x_o_L(long(i), j), x_o_U(long(i), j)});
            }

        // y-coord
        freeLineSegment.ty.push_back(ty[i]);

        // Set operations for Collision-free intervals at each line
        obsMerge = op.unions(obsSeg);
        arenaIntersect = op.intersects(arenaSeg);
        interval[i] = op.complements(arenaIntersect, obsMerge);

        // x-(z-)coords
        for (size_t j = 0; j < interval[i].size(); j++) {
            xL.push_back(interval[i][j].s());
            xU.push_back(interval[i][j].e());
            xM.push_back((interval[i][j].s() + interval[i][j].e()) / 2.0);
        }
        freeLineSegment.xL.push_back(xL);
        freeLineSegment.xU.push_back(xU);
        freeLineSegment.xM.push_back(xM);

        // Clear memory
        arenaSeg.clear();
        obsSeg.clear();
        xL.clear();
        xU.clear();
        xM.clear();
    }

    // Enhanced process to generate more valid vertices within free line
    // segement
    enhanceDecomp(&freeLineSegment);

    return freeLineSegment;
}

template <class RobotType, class ObjectType>
void HighwayRoadMap<RobotType, ObjectType>::enhanceDecomp(
    FreeSegment2D* freeSeg) {
    // Add new vertices within on sweep line
    for (size_t i = 0; i < freeSeg->ty.size() - 1; ++i) {
        for (size_t j1 = 0; j1 < freeSeg->xM[i].size(); ++j1) {
            for (size_t j2 = 0; j2 < freeSeg->xM[i + 1].size(); ++j2) {
                if (freeSeg->xM[i][j1] < freeSeg->xL[i + 1][j2] &&
                    freeSeg->xU[i][j1] >= freeSeg->xL[i + 1][j2]) {
                    freeSeg->xU[i].push_back(freeSeg->xL[i + 1][j2]);
                    freeSeg->xL[i].push_back(freeSeg->xL[i + 1][j2]);
                    freeSeg->xM[i].push_back(freeSeg->xL[i + 1][j2]);
                } else if (freeSeg->xM[i][j1] > freeSeg->xU[i + 1][j2] &&
                           freeSeg->xL[i][j1] <= freeSeg->xU[i + 1][j2]) {
                    freeSeg->xU[i].push_back(freeSeg->xU[i + 1][j2]);
                    freeSeg->xL[i].push_back(freeSeg->xU[i + 1][j2]);
                    freeSeg->xM[i].push_back(freeSeg->xU[i + 1][j2]);
                }

                if (freeSeg->xM[i + 1][j2] < freeSeg->xL[i][j1] &&
                    freeSeg->xU[i + 1][j2] >= freeSeg->xL[i][j1]) {
                    freeSeg->xU[i + 1].push_back(freeSeg->xL[i][j1]);
                    freeSeg->xL[i + 1].push_back(freeSeg->xL[i][j1]);
                    freeSeg->xM[i + 1].push_back(freeSeg->xL[i][j1]);
                } else if (freeSeg->xM[i + 1][j2] > freeSeg->xU[i][j1] &&
                           freeSeg->xL[i + 1][j2] <= freeSeg->xU[i][j1]) {
                    freeSeg->xU[i + 1].push_back(freeSeg->xU[i][j1]);
                    freeSeg->xL[i + 1].push_back(freeSeg->xU[i][j1]);
                    freeSeg->xM[i + 1].push_back(freeSeg->xU[i][j1]);
                }
            }
        }

        sort(freeSeg->xL[i].begin(), freeSeg->xL[i].end(),
             [](double a, double b) { return a < b; });
        sort(freeSeg->xU[i].begin(), freeSeg->xU[i].end(),
             [](double a, double b) { return a < b; });
        sort(freeSeg->xM[i].begin(), freeSeg->xM[i].end(),
             [](double a, double b) { return a < b; });
    }
}

#endif  // HIGHWAYROADMAP_INL_H

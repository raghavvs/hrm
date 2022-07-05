#include "planners/include/ompl_interface/OMPL2D.h"

#include "ompl/base/PrecomputedStateSampler.h"
#include "ompl/base/spaces/DubinsStateSpace.h"
#include "ompl/base/spaces/ReedsSheppStateSpace.h"
#include "ompl/base/spaces/SE2StateSpace.h"

OMPL2D::OMPL2D(MultiBodyTree2D robot, std::vector<SuperEllipse> arena,
               std::vector<SuperEllipse> obstacle)
    : robot_(std::move(robot)),
      arena_(std::move(arena)),
      obstacle_(std::move(obstacle)) {}

OMPL2D::~OMPL2D() = default;

void OMPL2D::setup(const Index spaceId, const Index plannerId,
                   const Index validStateSamplerId) {
    const double STATE_VALIDITY_RESOLUTION = 0.01;

    // Setup space bound
    setEnvBound();

    ob::StateSpacePtr space(std::make_shared<ob::SE2StateSpace>());
    if (spaceId == 1) {
        space = std::make_shared<ob::DubinsStateSpace>();
    } else if (spaceId == 2) {
        space = std::make_shared<ob::ReedsSheppStateSpace>();
    }

    ob::RealVectorBounds bounds(2);
    bounds.setLow(0, param_.xLim.first);
    bounds.setLow(1, param_.yLim.first);
    bounds.setHigh(0, param_.xLim.second);
    bounds.setHigh(1, param_.yLim.second);
    space->as<ob::SE2StateSpace>()->setBounds(bounds);

    // Setup planner
    ss_ = std::make_shared<og::SimpleSetup>(space);

    // Set collision checker
    ss_->setStateValidityChecker(
        [this](const ob::State *state) { return isStateValid(state); });
    ss_->getSpaceInformation()->setStateValidityCheckingResolution(
        STATE_VALIDITY_RESOLUTION);
    setCollisionObject();

    setPlanner(plannerId);
    setValidStateSampler(validStateSamplerId);
}

void OMPL2D::plan(const std::vector<std::vector<Coordinate>> &endPts) {
    const double DEFAULT_PLAN_TIME = 60.0;
    numCollisionChecks_ = 0;

    // Set start and goal poses
    ob::ScopedState<ob::SE2StateSpace> start(ss_->getSpaceInformation());
    start->setX(endPts[0][0]);
    start->setY(endPts[0][1]);
    start->setYaw(endPts[0][2]);
    start.enforceBounds();

    ob::ScopedState<ob::SE2StateSpace> goal(ss_->getSpaceInformation());
    goal->setX(endPts[1][0]);
    goal->setY(endPts[1][1]);
    goal->setYaw(endPts[1][2]);
    goal.enforceBounds();

    ss_->setStartAndGoalStates(start, goal);

    // Solve the planning problem
    try {
        isSolved_ = ss_->solve(DEFAULT_PLAN_TIME);
    } catch (ompl::Exception &ex) {
    }

    getSolution();
    ss_->clear();
}

void OMPL2D::getSolution() {
    if (isSolved_) {
        const unsigned int INTERPOLATION_NUMBER = 200;

        // Get solution path
        ss_->simplifySolution();
        auto path = ss_->getSolutionPath();
        lengthPath_ = ss_->getSolutionPath().getStates().size();

        // Save interpolated path
        path.interpolate(INTERPOLATION_NUMBER);
        for (auto *state : path.getStates()) {
            path_.push_back(
                {state->as<ob::SE2StateSpace::StateType>()->getX(),
                 state->as<ob::SE2StateSpace::StateType>()->getY(),
                 state->as<ob::SE2StateSpace::StateType>()->getYaw()});
        }
    }

    // Retrieve planning data
    totalTime_ = ss_->getLastPlanComputationTime();

    ob::PlannerData pd(ss_->getSpaceInformation());
    ss_->getPlannerData(pd);

    // Number of vertices, edges
    numValidStates_ = pd.numVertices();
    numValidEdges_ = pd.numEdges();

    // Save vertices and edges
    const ob::State *state;
    vertex_.clear();
    for (auto i = 0; i < numValidStates_; ++i) {
        state = pd.getVertex(i).getState()->as<ob::State>();
        vertex_.push_back(
            {state->as<ob::SE2StateSpace::StateType>()->getX(),
             state->as<ob::SE2StateSpace::StateType>()->getY(),
             state->as<ob::SE2StateSpace::StateType>()->getYaw()});
    }

    std::vector<std::vector<unsigned int>> edgeInfo(numValidStates_);
    edge_.clear();
    for (auto i = 0; i < numValidStates_; i++) {
        pd.getEdges(i, edgeInfo[i]);
        for (auto edgeI : edgeInfo[i]) {
            edge_.emplace_back(std::make_pair(i, edgeI));
        }
    }
}

void OMPL2D::setEnvBound() {
    // Setup parameters
    param_.numY = 50;

    double f = 1;
    std::vector<Coordinate> bound = {
        arena_.at(0).getSemiAxis().at(0) -
            f * robot_.getBase().getSemiAxis().at(0),
        arena_.at(0).getSemiAxis().at(1) -
            f * robot_.getBase().getSemiAxis().at(0)};
    param_.xLim = {arena_.at(0).getPosition().at(0) - bound.at(0),
                   arena_.at(0).getPosition().at(0) + bound.at(0)};
    param_.yLim = {arena_.at(0).getPosition().at(1) - bound.at(1),
                   arena_.at(0).getPosition().at(1) + bound.at(1)};
}

void OMPL2D::setPlanner(const Index plannerId) {
    // Set the planner
    if (plannerId == 0) {
        ss_->setPlanner(std::make_shared<og::PRM>(ss_->getSpaceInformation()));
    } else if (plannerId == 1) {
        ss_->setPlanner(
            std::make_shared<og::LazyPRM>(ss_->getSpaceInformation()));
    } else if (plannerId == 2) {
        ss_->setPlanner(std::make_shared<og::RRT>(ss_->getSpaceInformation()));
    } else if (plannerId == 3) {
        ss_->setPlanner(
            std::make_shared<og::RRTConnect>(ss_->getSpaceInformation()));
    } else if (plannerId == 4) {
        ss_->setPlanner(std::make_shared<og::EST>(ss_->getSpaceInformation()));
    } else if (plannerId == 5) {
        ss_->setPlanner(std::make_shared<og::SBL>(ss_->getSpaceInformation()));
    } else if (plannerId == 6) {
        ss_->setPlanner(
            std::make_shared<og::KPIECE1>(ss_->getSpaceInformation()));
    }
}

void OMPL2D::setValidStateSampler(const Index validSamplerId) {
    // Set the valid state sampler
    if (validSamplerId == 2) {
        // Uniform sampler
        ss_->getSpaceInformation()->setValidStateSamplerAllocator(
            [](const ob::SpaceInformation *si) -> ob::ValidStateSamplerPtr {
                return std::make_shared<ob::UniformValidStateSampler>(si);
            });
    } else if (validSamplerId == 3) {
        // Gaussian sampler
        ss_->getSpaceInformation()->setValidStateSamplerAllocator(
            [](const ob::SpaceInformation *si) -> ob::ValidStateSamplerPtr {
                return std::make_shared<ob::GaussianValidStateSampler>(si);
            });
    } else if (validSamplerId == 4) {
        // Obstacle-based sampler
        ss_->getSpaceInformation()->setValidStateSamplerAllocator(
            [](const ob::SpaceInformation *si) -> ob::ValidStateSamplerPtr {
                return std::make_shared<ob::ObstacleBasedValidStateSampler>(si);
            });
    } else if (validSamplerId == 5) {
        // Maximum-clearance sampler
        ss_->getSpaceInformation()->setValidStateSamplerAllocator(
            [](const ob::SpaceInformation *si) -> ob::ValidStateSamplerPtr {
                auto vss =
                    std::make_shared<ob::MaximizeClearanceValidStateSampler>(
                        si);
                vss->setNrImproveAttempts(5);
                return vss;
            });
    } else if (validSamplerId == 6) {
        // Bridge-test sampler
        ss_->getSpaceInformation()->setValidStateSamplerAllocator(
            [](const ob::SpaceInformation *si) -> ob::ValidStateSamplerPtr {
                return std::make_shared<ob::BridgeTestValidStateSampler>(si);
            });
    }

    ss_->setup();
}

void OMPL2D::setCollisionObject() {
    // Setup collision object for ellipsoidal robot parts
    robotGeom_.push_back(setCollisionObjectFromSQ(robot_.getBase()));
    for (size_t i = 0; i < robot_.getNumLinks(); ++i) {
        robotGeom_.push_back(setCollisionObjectFromSQ(robot_.getLinks().at(i)));
    }

    // Setup collision object for superquadric obstacles
    for (const auto &obs : obstacle_) {
        obsGeom_.push_back(setCollisionObjectFromSQ(obs));
    }
}

bool OMPL2D::isStateValid(const ob::State *state) {
    // Get pose info the transform the robot
    const Coordinate x = state->as<ob::SE2StateSpace::StateType>()->getX();
    const Coordinate y = state->as<ob::SE2StateSpace::StateType>()->getY();
    const double th = state->as<ob::SE2StateSpace::StateType>()->getYaw();

    Eigen::Matrix3d tf = Eigen::Matrix3d::Identity();
    tf.topLeftCorner(2, 2) = Eigen::Rotation2Dd(th).toRotationMatrix();
    tf.topRightCorner(2, 1) = Eigen::Array2d({x, y});

    robot_.robotTF(tf);

    // Checking collision with obstacles
    for (size_t i = 0; i < obstacle_.size(); ++i) {
        if (std::fabs(obstacle_.at(i).getEpsilon() - 1.0) < 1e-6) {
            // For two ellipses, use ASC algorithm
            if (!isEllipseSeparated(robot_.getBase(), obstacle_.at(i))) {
                return false;
            }

            for (size_t j = 0; j < robot_.getNumLinks(); ++j) {
                if (!isEllipseSeparated(robot_.getLinks().at(j),
                                        obstacle_.at(i))) {
                    return false;
                }
            }
        } else {
            // For an ellipse and superellipse, use FCL
            if (isCollision(robot_.getBase(), &robotGeom_.at(0),
                            obstacle_.at(i), &obsGeom_.at(i))) {
                return false;
            }

            for (size_t j = 0; j < robot_.getNumLinks(); ++j) {
                if (isCollision(robot_.getLinks().at(j), &robotGeom_.at(j + 1),
                                obstacle_.at(i), &obsGeom_.at(i))) {
                    return false;
                }
            }
        }
    }
    numCollisionChecks_++;

    return true;
}

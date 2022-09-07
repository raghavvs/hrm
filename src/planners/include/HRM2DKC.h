#pragma once

#include "HRM2D.h"
#include "geometry/include/PointInPoly.h"

/** \class HRM2DKC
 * \brief Highway RoadMap planner for 2D single-body robot using Kinematics of
 * Containment (KC) for slice connections */
class HRM2DKC : public HRM2D {
  public:
    /** \brief Constructor
     * \param robot MultibodyTree type defining the robot
     * \param arena vector of geometric types definint the planning arena
     * \param obs vector of geometric types defining obstacles
     * \param req PlanningRequest structure */
    HRM2DKC(const MultiBodyTree2D& robot,
            const std::vector<SuperEllipse>& arena,
            const std::vector<SuperEllipse>& obs, const PlanningRequest& req);

    ~HRM2DKC();

    void connectMultiLayer() override;

  private:
    /** \brief Add a middle vertex between two vertices for edge connection
     * \param vtx1 Start vertex
     * \param vtx2 Goal vertex
     * \return The generated middle vertex */
    std::vector<double> addMiddleVertex(std::vector<Coordinate> vtx1,
                                        std::vector<Coordinate> vtx2);

    /** \param Descriptions of polyhedral local C-space */
    PolyCSpace polyVtx;

    /** \param Inflation factor for the robot */
    double infla_ = 0.1;
};

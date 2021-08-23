#pragma once

#include "HRM3D.h"
#include "util/include/ParseURDF.h"

/** \class ProbHRM3D Prob-HRM for 3D robot planning */
class ProbHRM3D : public HRM3D {
  public:
    ProbHRM3D(const MultiBodyTree3D& robot, const std::string urdfFile,
              const std::vector<SuperQuadrics>& arena,
              const std::vector<SuperQuadrics>& obs,
              const PlanningRequest& req);

    ~ProbHRM3D() override;

  public:
    void plan(const double timeLim) override;

    void sampleOrientations() override;

    void connectMultiLayer() override;

    void generateVertices(const double tx,
                          const FreeSegment2D* freeSeg) override;

  protected:
    void setTransform(const std::vector<double>& v) override;

    void computeTFE(const std::vector<double>& v1,
                    const std::vector<double>& v2,
                    std::vector<SuperQuadrics>* tfe);

  private:
    ParseURDF* kdl_;
    std::string urdfFile_;
    const double maxJointAngle_ = pi / 2;

    // store configuration for each robot shape (at each C-layer)
    std::vector<std::vector<double>> v_;
};

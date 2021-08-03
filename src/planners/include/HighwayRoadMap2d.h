#ifndef HIGHWAYROADMAP2D_H
#define HIGHWAYROADMAP2D_H

#include "HighwayRoadMap.h"
#include "src/geometry/include/TightFitEllipsoid.h"
#include "util/include/MultiBodyTree2D.h"

class HighwayRoadMap2D : public HighwayRoadMap<MultiBodyTree2D, SuperEllipse> {
  public:
    HighwayRoadMap2D(const MultiBodyTree2D& robot,
                     const std::vector<SuperEllipse>& arena,
                     const std::vector<SuperEllipse>& obs,
                     const PlanningRequest& req);

    virtual ~HighwayRoadMap2D() override;

  public:
    /** \brief get free line segment at one specific C-layer
     * \param Boundary pointer to Minkowski boundaries
     * \return FreeSegment2D
     */
    FreeSegment2D getFreeSegmentOneLayer(const Boundary* bd) {
        sweepLineProcess(bd);
        return freeSegOneLayer_;
    }

    virtual void buildRoadmap() override;

    virtual void connectMultiLayer() override;

    virtual void generateVertices(const double tx,
                                  const FreeSegment2D* freeSeg) override;

    void sweepLineProcess(const Boundary* bd) override;

  protected:
    /** \brief bridgeLayer generating bridge C-layer to connect adjacent
     * C-layers
     * \param SuperEllipse TFE for robot bodies
     * \return Minkowski operation boundaries
     */
    Boundary bridgeLayer(SuperEllipse Ec);

    bool isSameLayerTransitionFree(const std::vector<double>& v1,
                                   const std::vector<double>& v2) override;

    bool isMultiLayerTransitionFree(const std::vector<double>& v1,
                                    const std::vector<double>& v2) override;

    bool isPtInCFree(const Boundary* bd, const std::vector<double>& v);

    std::vector<Vertex> getNearestNeighborsOnGraph(
        const std::vector<double>& vertex, const size_t k,
        const double radius) override;

    virtual void setTransform(const std::vector<double>& v) override;

    virtual void computeTFE(const double thetaA, const double thetaB,
                            std::vector<SuperEllipse>* tfe);

  protected:
    /** \brief ang_r sampled orientations of the robot */
    std::vector<double> ang_r;

    Boundary layerBound_;

    FreeSegment2D freeSegOneLayer_;

    /** \brief tfe_ TFE for bridge C-layers */
    std::vector<SuperEllipse> tfe_;

    /** \brief Minkowski operation boundaries in bridge C-layer */
    std::vector<Boundary> bridgeLayerBound_;
};

#endif  // HIGHWAYROADMAP2D_H

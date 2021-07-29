#ifndef HIGHWAYROADMAP3D_H
#define HIGHWAYROADMAP3D_H

#include "src/geometry/include/TightFitEllipsoid.h"
#include "src/planners/include/HighwayRoadMap.h"
#include "util/include/InterpolateSE3.h"
#include "util/include/LineIntersection.h"

#include <ompl/datastructures/NearestNeighbors.h>

// FreeSegment3D collision-free line segments in 3D
struct FreeSegment3D {
    std::vector<double> tx;
    std::vector<FreeSegment2D> cellYZ;
};

/** \brief vertexIdx vertex index at each C-layer, sweep line */
struct vertexIdx {
    size_t layer;
    std::vector<size_t> plane;
    std::vector<std::vector<size_t>> line;
};

class HighwayRoadMap3D : public HighwayRoadMap<SuperQuadrics, SuperQuadrics> {
  public:
    HighwayRoadMap3D(const SuperQuadrics& robot,
                     const std::vector<SuperQuadrics>& arena,
                     const std::vector<SuperQuadrics>& obs,
                     const PlanningRequest& req);

    virtual ~HighwayRoadMap3D();

  public:
    virtual void buildRoadmap() override;
    virtual Boundary boundaryGen() override;

    /**
     * \brief sweep-line process to explore free space in one C-layer
     * \param Boundary Minkowski boundry of obstacles and arenas
     * \return collision-free cells info
     */
    FreeSegment3D sweepLine3D(const Boundary* bd);

    /**
     * \brief subroutine for generating collision-free vertices on the yz-plane
     * \param ty a vector of incremented y-coordinates
     * \param z_s_L, z_s_U, z_o_L, z_o_U upper(U) and lower(L) z-coordinates of
     * the intersection for arena(s) and obstacles(o), size = num of
     * y-coordinates X num of objects
     * \return collision-free cells info
     */
    virtual void generateVertices(const double tx, const FreeSegment2D* cellYZ);

    /** \brief connect within one C-layer */
    void connectOneLayer3D(const FreeSegment3D* cell);

    /** \brief subroutine to first connect vertices within on sweep-plane
     * (vertical to x-axis) */
    void connectOneLayer2D(const FreeSegment2D* cellYZ) override;

    /** \brief connect within adjacent C-layers, using the idea of "bridge
     * C-layer" */
    virtual void connectMultiLayer() override;

    /** \brief subroutine to first construct the middle C-layer */
    //    FreeSegment3D bridgeLayer(SuperQuadrics);
    std::vector<MeshMatrix> bridgeLayer(SuperQuadrics Ec);

    /** \brief get the resulting solved path and the interpolated one */
    std::vector<std::vector<double>> getInterpolatedSolutionPath(
        const unsigned int num);

  protected:
    bool isSameLayerTransitionFree(const std::vector<double>& v1,
                                   const std::vector<double>& v2) override;

    virtual bool isMultiLayerTransitionFree(
        const std::vector<double>& v1, const std::vector<double>& v2) override;

    std::vector<Vertex> getNearestNeighborsOnGraph(
        const std::vector<double>& vertex, const size_t k,
        const double radius) override;

    /** \brief check is one point is within C-free */
    bool isPtInCFree(const std::vector<MeshMatrix>* bdMesh,
                     const std::vector<double>& V);

    /** \brief uniform random sample SO(3) */
    void sampleSO3();

    virtual void setTransform(const std::vector<double>& v) override;

  public:
    /** \param q_r sampled orientations (Quaternion) of the robot */
    std::vector<Eigen::Quaterniond> q_r;

    // Vertex index info
    std::vector<vertexIdx> vtxId;
    vertexIdx N_v;

  protected:
    std::vector<MeshMatrix> CLayerBound;
    std::vector<SuperQuadrics> tfe_;
    std::vector<MeshMatrix> bridgeLayerBound;
    std::vector<double> midVtx;
};

#endif  // HIGHWAYROADMAP3D_H

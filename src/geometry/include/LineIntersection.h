#ifndef LINEINTERSECTION_H
#define LINEINTERSECTION_H

#include "MeshGenerator.h"

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <vector>

std::vector<Eigen::Vector3d> intersectLineMesh3d(const Eigen::VectorXd& line,
                                                 const MeshMatrix& shape);

std::vector<Eigen::Vector3d> intersectVerticalLineMesh3d(
    const Eigen::VectorXd& line, const MeshMatrix& shape);

bool intersectLineTriangle3d(const Eigen::VectorXd* line,
                             const Eigen::Vector3d* t0,
                             const Eigen::Vector3d* u, const Eigen::Vector3d* v,
                             Eigen::Vector3d* pt);

bool isIntersectSegPolygon2D(
    const std::pair<std::vector<double>, std::vector<double>>& seg,
    const Eigen::Matrix2Xd& shape);

std::vector<double> intersectHorizontalLinePolygon2d(
    const double ty, const Eigen::Matrix2Xd& shape);

#endif  // LINEINTERSECTION_H

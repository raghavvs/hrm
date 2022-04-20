#pragma once

#include "eigen3/Eigen/Dense"

using Coordinate = double;
using BoundaryPoints = Eigen::MatrixXd;
using Indicator = int;
using Index = size_t;
using Point3D = Eigen::Vector3d;
using Line3D = Eigen::VectorXd;

using SE2Transform = Eigen::Matrix3d;
using SE3Transform = Eigen::Matrix4d;
